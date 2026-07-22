#include "rdppane.h"
#include "config.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

// ---------------------------------------------------------------------------
// WinEvent hook — fires in our UI thread (WINEVENT_OUTOFCONTEXT) the instant
// mstsc calls ShowWindow on TscShellContainerClass.  We hide the window
// immediately so it is never composited as a standalone frame before embedding.
// Keyed by mstsc PID so multiple concurrent RDP tabs don't interfere.
// ---------------------------------------------------------------------------

static QHash<DWORD, RdpPane*> s_paneByPid;

static void CALLBACK rdpWinEventProc(
    HWINEVENTHOOK, DWORD, HWND hwnd,
    LONG idObject, LONG, DWORD, DWORD)
{
    if (idObject != OBJID_WINDOW) return;
    WCHAR cls[256] = {};
    GetClassNameW(hwnd, cls, 256);
    if (_wcsicmp(cls, L"TscShellContainerClass") != 0) return;
    // The hook fires for both the early hidden placeholder and the real session
    // window.  IsWindowVisible distinguishes them — the placeholder stays hidden
    // until the session is established; the real window is shown immediately.
    if (!IsWindowVisible(hwnd)) return;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    RdpPane *pane = s_paneByPid.value(pid, nullptr);
    if (pane) pane->embedWindow(reinterpret_cast<WId>(hwnd));
}

// ---------------------------------------------------------------------------
// EnumWindows fallback — used only when the WinEvent hook hasn't fired
// (e.g. message delivery lag or hook registration edge cases).
// ---------------------------------------------------------------------------

struct RdpEnumData { const QSet<quintptr> *exclude; HWND found; };

static BOOL CALLBACK findRdpSession(HWND hwnd, LPARAM lp)
{
    auto *d = reinterpret_cast<RdpEnumData *>(lp);
    if (d->exclude->contains(reinterpret_cast<quintptr>(hwnd))) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;
    WCHAR cls[256] = {};
    GetClassNameW(hwnd, cls, 256);
    if (_wcsicmp(cls, L"TscShellContainerClass") == 0) { d->found = hwnd; return FALSE; }
    return TRUE;
}

static BOOL CALLBACK snapshotRdpSessions(HWND hwnd, LPARAM lp)
{
    auto *s = reinterpret_cast<QSet<quintptr> *>(lp);
    WCHAR cls[256] = {};
    GetClassNameW(hwnd, cls, 256);
    if (_wcsicmp(cls, L"TscShellContainerClass") == 0)
        s->insert(reinterpret_cast<quintptr>(hwnd));
    return TRUE;
}

// ---------------------------------------------------------------------------
// Run a console utility without flashing a console window.
// ---------------------------------------------------------------------------
static void runHidden(const QString &program, const QStringList &args)
{
    QProcess proc;
#ifdef Q_OS_WIN
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        a->flags |= CREATE_NO_WINDOW;
    });
#endif
    proc.start(program, args);
    proc.waitForFinished(5000);
}

// ---------------------------------------------------------------------------

RdpPane::RdpPane(const QJsonObject &session, QWidget *parent)
    : QWidget(parent)
{
    name   = session.value("name").toString();
    m_host = session.value("host").toString();
    m_port = session.value("port").toInt(3389);
    m_user = session.value("username").toString();

    setAttribute(Qt::WA_NativeWindow);

    m_status = new QLabel(QString("Connecting to %1...").arg(m_host));
    m_status->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_status);
}

void RdpPane::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_initialized) {
        m_initialized = true;
        QTimer::singleShot(0, this, &RdpPane::connectToHost);
    }
}

void RdpPane::connectToHost()
{
    if (m_pollTimer) { m_pollTimer->deleteLater(); m_pollTimer = nullptr; }
    if (m_process)   { m_process->deleteLater();   m_process   = nullptr; }

    if (m_cachedPass.isEmpty()) {
        QDialog credDlg;
        credDlg.setWindowTitle(QString("Connect to %1").arg(m_host));
        auto *form = new QFormLayout(&credDlg);

        auto *userEdit = new QLineEdit(m_user, &credDlg);
        auto *passEdit = new QLineEdit(&credDlg);
        passEdit->setEchoMode(QLineEdit::Password);
        form->addRow("Username:", userEdit);
        form->addRow("Password:", passEdit);

        auto *btns = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &credDlg);
        form->addRow(btns);
        connect(btns, &QDialogButtonBox::accepted, &credDlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &credDlg, &QDialog::reject);

        if (credDlg.exec() != QDialog::Accepted) {
            m_status->setText("Connection cancelled.");
            return;
        }
        m_user       = userEdit->text();
        m_cachedPass = passEdit->text();
    }

    m_credKey = QString("TERMSRV/%1").arg(m_host);
    runHidden("cmdkey.exe", {
        QString("/generic:%1").arg(m_credKey),
        QString("/user:%1").arg(m_user),
        QString("/pass:%1").arg(m_cachedPass)
    });

    m_existingWindows.clear();
    EnumWindows(snapshotRdpSessions, reinterpret_cast<LPARAM>(&m_existingWindows));

    const qreal dpr = devicePixelRatioF();
    const int pw = qRound(width()  * dpr);
    const int ph = qRound(height() * dpr);

    QStringList args;
    args << QString("/v:%1:%2").arg(m_host).arg(m_port);
    if (pw > 0 && ph > 0)
        args << QString("/w:%1").arg(pw) << QString("/h:%1").arg(ph);

    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &RdpPane::onProcessFinished);
    m_process->start("mstsc.exe", args);

    if (!m_process->waitForStarted(5000)) {
        m_status->setText("Failed to launch mstsc.exe.");
        return;
    }

    // Register a WinEvent hook on this specific mstsc PID so we are notified
    // the instant TscShellContainerClass becomes visible — before DWM composites
    // even one frame of the standalone window.
    DWORD pid = static_cast<DWORD>(m_process->processId());
    if (pid) {
        s_paneByPid[pid] = this;
        m_winEventHook = reinterpret_cast<quintptr>(
            SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW,
                            nullptr, rdpWinEventProc, pid, 0,
                            WINEVENT_OUTOFCONTEXT));
    }

    // Fallback poll in case the hook delivery is delayed.
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &RdpPane::pollForWindow);
    m_pollTimer->start(100);
    QTimer::singleShot(300000, m_pollTimer, &QTimer::stop);
}

void RdpPane::embedWindow(WId hwndId)
{
    if (m_mstscHwnd) return; // Already embedded (hook and poll both fire — ignore second)

    if (m_pollTimer) m_pollTimer->stop();

    // Unhook now that we have the window — no more events needed.
    if (m_winEventHook) {
        UnhookWinEvent(reinterpret_cast<HWINEVENTHOOK>(m_winEventHook));
        m_winEventHook = 0;
        if (m_process)
            s_paneByPid.remove(static_cast<DWORD>(m_process->processId()));
    }

    HWND hwnd = reinterpret_cast<HWND>(hwndId);

    // Hide immediately — when called from the WinEvent hook the window has just
    // become visible but has not yet been composited; from the fallback poll it
    // may already have been painted once.  Either way, hide before embedding.
    ShowWindow(hwnd, SW_HIDE);

    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
               WS_MAXIMIZEBOX | WS_SYSMENU | WS_BORDER);
    style |= WS_CHILD;
    SetWindowLong(hwnd, GWL_STYLE, style);

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                 WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

    HWND ours = reinterpret_cast<HWND>(winId());
    SetParent(hwnd, ours);
    m_mstscHwnd = hwndId;

    const qreal dpr = devicePixelRatioF();
    SetWindowPos(hwnd, HWND_TOP, 0, 0,
                 qRound(width() * dpr), qRound(height() * dpr),
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    m_status->hide();

    if (!m_credKey.isEmpty()) {
        runHidden("cmdkey.exe", { QString("/delete:%1").arg(m_credKey) });
        m_credKey.clear();
    }

    m_statsHost = m_host;
    m_statsUser = m_user;
    m_statsPass = m_cachedPass;
    startStatsPolling();
}

void RdpPane::reconnect()
{
    disconnectRdp();
    m_status->setText(QString("Reconnecting to %1...").arg(m_host));
    m_status->show();
    QTimer::singleShot(0, this, &RdpPane::connectToHost);
}

RdpPane::~RdpPane()
{
    if (m_winEventHook) {
        UnhookWinEvent(reinterpret_cast<HWINEVENTHOOK>(m_winEventHook));
        if (m_process)
            s_paneByPid.remove(static_cast<DWORD>(m_process->processId()));
    }
    if (m_mstscHwnd) {
        HWND hwnd = reinterpret_cast<HWND>(m_mstscHwnd);
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        style &= ~WS_CHILD;
        style |= WS_POPUP;
        SetWindowLong(hwnd, GWL_STYLE, style);
        SetParent(hwnd, nullptr);
        m_mstscHwnd = 0;
    }
    if (m_process) {
        m_process->disconnect();
        if (m_process->state() != QProcess::NotRunning)
            m_process->kill();
    }
    if (!m_credKey.isEmpty())
        runHidden("cmdkey.exe", { QString("/delete:%1").arg(m_credKey) });
}

void RdpPane::pollForWindow()
{
    // Fallback for cases where the WinEvent hook delivery is delayed.
    // embedWindow guards against double-call if the hook already fired.
    RdpEnumData data { &m_existingWindows, nullptr };
    EnumWindows(findRdpSession, reinterpret_cast<LPARAM>(&data));
    if (data.found)
        embedWindow(reinterpret_cast<WId>(data.found));
}

void RdpPane::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_mstscHwnd) return;
    if (event->size().width() <= 0 || event->size().height() <= 0) return;

    // Resize the HWND immediately so the session fills the tab during the drag,
    // then reconnect at the correct resolution 500 ms after the drag settles.
    // mstsc stretches its content to fill the HWND during the drag; the
    // reconnect (using cached credentials) restores the native resolution.
    HWND hwnd = reinterpret_cast<HWND>(m_mstscHwnd);
    const qreal dpr = devicePixelRatioF();
    SetWindowPos(hwnd, nullptr,
                 0, 0,
                 qRound(event->size().width()  * dpr),
                 qRound(event->size().height() * dpr),
                 SWP_NOZORDER | SWP_NOACTIVATE);

    if (!m_resizeTimer) {
        m_resizeTimer = new QTimer(this);
        m_resizeTimer->setSingleShot(true);
        connect(m_resizeTimer, &QTimer::timeout, this, &RdpPane::reconnect);
    }
    m_resizeTimer->start(500);
}

void RdpPane::onProcessFinished()
{
    if (m_pollTimer) m_pollTimer->stop();
    if (m_winEventHook) {
        UnhookWinEvent(reinterpret_cast<HWINEVENTHOOK>(m_winEventHook));
        m_winEventHook = 0;
        if (m_process)
            s_paneByPid.remove(static_cast<DWORD>(m_process->processId()));
    }
    m_mstscHwnd = 0;
    m_cachedPass.clear();
    stopStatsPolling();
    m_status->setText(QString("Disconnected from %1.").arg(m_host));
    m_status->show();
}

void RdpPane::disconnectRdp()
{
    if (m_pollTimer) m_pollTimer->stop();
    if (m_resizeTimer) m_resizeTimer->stop();
    stopStatsPolling();

    if (m_winEventHook) {
        UnhookWinEvent(reinterpret_cast<HWINEVENTHOOK>(m_winEventHook));
        m_winEventHook = 0;
        if (m_process)
            s_paneByPid.remove(static_cast<DWORD>(m_process->processId()));
    }

    if (!m_credKey.isEmpty()) {
        runHidden("cmdkey.exe", { QString("/delete:%1").arg(m_credKey) });
        m_credKey.clear();
    }

    if (m_mstscHwnd) {
        HWND hwnd = reinterpret_cast<HWND>(m_mstscHwnd);
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        style &= ~WS_CHILD;
        style |= WS_POPUP;
        SetWindowLong(hwnd, GWL_STYLE, style);
        SetParent(hwnd, nullptr);
        m_mstscHwnd = 0;
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }

    if (m_process) {
        m_process->disconnect();
        if (m_process->state() != QProcess::NotRunning)
            m_process->kill();
    }
}

// ---------------------------------------------------------------------------
// Remote stats polling via WMI (processor queue length, RAM, page file)
// ---------------------------------------------------------------------------

void RdpPane::startStatsPolling() {
    if (m_statsTimer) return;
    m_statsTimer = new QTimer(this);
    connect(m_statsTimer, &QTimer::timeout, this, &RdpPane::pollStats);
    m_statsTimer->start(2000);
    pollStats();
}

void RdpPane::stopStatsPolling() {
    if (m_statsTimer) { m_statsTimer->stop(); m_statsTimer->deleteLater(); m_statsTimer = nullptr; }
    if (m_statsProcess) {
        m_statsProcess->disconnect(this);
        m_statsProcess->kill();
        m_statsProcess->deleteLater();
        m_statsProcess = nullptr;
    }
    lastStats = {};
}

void RdpPane::pollStats() {
    if (m_statsProcess && m_statsProcess->state() != QProcess::NotRunning) return;
    if (m_statsHost.isEmpty() || m_statsPass.isEmpty()) return;

    QString pass = m_statsPass;
    pass.replace("'", "''");  // escape single quotes for PowerShell string literal

    QString cmd = QString(
        "$pass = ConvertTo-SecureString '%1' -AsPlainText -Force; "
        "$cred = New-Object PSCredential('%2', $pass); "
        "$cpu = Get-WmiObject -ComputerName '%3' -Credential $cred "
            "-Class Win32_PerfFormattedData_PerfOS_Processor "
            "-Filter \"Name='_Total'\" -ErrorAction Stop; "
        "$sys = Get-WmiObject -ComputerName '%3' -Credential $cred "
            "-Class Win32_PerfFormattedData_PerfOS_System -ErrorAction Stop; "
        "$os = Get-WmiObject -ComputerName '%3' -Credential $cred "
            "-Class Win32_OperatingSystem -ErrorAction Stop; "
        "Write-Output \"$($cpu.PercentProcessorTime) $($sys.ProcessorQueueLength) "
            "$($os.FreePhysicalMemory) $($os.TotalVisibleMemorySize) "
            "$($os.FreeSpaceInPagingFiles) $($os.SizeStoredInPagingFiles)\""
    ).arg(pass, m_statsUser, m_statsHost);

    QProcess *proc = new QProcess(this);
#ifdef Q_OS_WIN
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        a->flags |= CREATE_NO_WINDOW;
    });
#endif
    connect(proc, &QProcess::finished, this,
            [this, proc](int exitCode, QProcess::ExitStatus) {
        if (m_statsProcess == proc) m_statsProcess = nullptr;
        if (exitCode != 0) { proc->deleteLater(); return; }

        QString output = proc->readAllStandardOutput().trimmed();
        proc->deleteLater();

        QStringList parts = output.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 6) return;

        bool ok;
        int       cpuPct   = parts[0].toInt(&ok);       if (!ok) return;
        int       procQ    = parts[1].toInt(&ok);        if (!ok) return;
        long long freeMem  = parts[2].toLongLong(&ok);  if (!ok) return;
        long long totalMem = parts[3].toLongLong(&ok);  if (!ok) return;
        long long freePF   = parts[4].toLongLong(&ok);  if (!ok) return;
        long long totalPF  = parts[5].toLongLong(&ok);  if (!ok) return;

        double ramPct = (totalMem > 0) ? (totalMem - freeMem) * 100.0 / totalMem : 0.0;
        double pfPct  = (totalPF  > 0) ? (totalPF  - freePF)  * 100.0 / totalPF  : 0.0;

        QJsonObject stats;
        stats["rdp"]   = true;
        stats["cpu"]   = cpuPct;
        stats["procq"] = procQ;
        stats["ram"]   = ramPct;
        stats["pf"]    = pfPct;
        lastStats = stats;
        emit statsReady(stats);
    });
    m_statsProcess = proc;
    proc->start("powershell.exe", {"-NonInteractive", "-NoProfile", "-Command", cmd});
}
