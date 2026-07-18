#include "rdppane.h"
#include "config.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
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
// EnumWindows callbacks — locate TscShellContainerClass (mstsc's session
// container window, created only after the connection is established).
// ---------------------------------------------------------------------------

struct RdpEnumData { const QSet<quintptr> *exclude; HWND found; };

static BOOL CALLBACK findRdpSession(HWND hwnd, LPARAM lp)
{
    auto *d = reinterpret_cast<RdpEnumData *>(lp);
    if (d->exclude->contains(reinterpret_cast<quintptr>(hwnd))) return TRUE;
    // Only match visible windows — mstsc sets WS_VISIBLE on TscShellContainerClass
    // only after the session is established and ready to display content.
    // Matching hidden windows picks up an unready placeholder that mstsc creates
    // early in the connection sequence, causing a blank embed and missing the
    // real session window entirely.
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
        // Defer one event loop tick so Qt finishes laying out the tab widget
        // and width()/height() return the actual usable dimensions.
        QTimer::singleShot(0, this, &RdpPane::connectToHost);
    }
}

void RdpPane::connectToHost()
{
    // Clean up any previous timer/process from a prior connection attempt.
    if (m_pollTimer) { m_pollTimer->deleteLater(); m_pollTimer = nullptr; }
    if (m_process)   { m_process->deleteLater();   m_process   = nullptr; }

    // Credential dialog — skipped when credentials are cached (automatic
    // reconnect triggered by a resize).  Always shown on first connect and
    // after the session has been closed by the user.
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

    // Store credentials in Windows Credential Manager so mstsc authenticates
    // silently via NLA without showing its own credential dialog.
    m_credKey = QString("TERMSRV/%1").arg(m_host);
    runHidden("cmdkey.exe", {
        QString("/generic:%1").arg(m_credKey),
        QString("/user:%1").arg(m_user),
        QString("/pass:%1").arg(m_cachedPass)
    });

    // Snapshot existing TscShellContainerClass windows so we don't steal one.
    m_existingWindows.clear();
    EnumWindows(snapshotRdpSessions, reinterpret_cast<LPARAM>(&m_existingWindows));

    // Session resolution in physical pixels so the remote desktop exactly
    // fills the HWND — mstsc renders at the pixel dimensions passed via /w /h.
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

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &RdpPane::pollForWindow);
    m_pollTimer->start(100);
    QTimer::singleShot(300000, m_pollTimer, &QTimer::stop);
}

void RdpPane::reconnect()
{
    disconnectRdp();
    m_status->setText(QString("Reconnecting to %1...").arg(m_host));
    m_status->show();
    // Cached credentials are reused — no credential prompt.
    QTimer::singleShot(0, this, &RdpPane::connectToHost);
}

RdpPane::~RdpPane()
{
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
    RdpEnumData data { &m_existingWindows, nullptr };
    EnumWindows(findRdpSession, reinterpret_cast<LPARAM>(&data));
    if (!data.found) return;

    m_pollTimer->stop();

    HWND hwnd = data.found;

    // Hide immediately — before stripping styles or calling SetParent — so
    // the window is invisible for as little time as possible as a standalone.
    // It will be shown again (embedded) via SWP_SHOWWINDOW in SetWindowPos.
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
    m_mstscHwnd = reinterpret_cast<WId>(hwnd);

    const qreal dpr = devicePixelRatioF();
    SetWindowPos(hwnd, HWND_TOP, 0, 0,
                 qRound(width() * dpr), qRound(height() * dpr),
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    m_status->hide();

    // NLA auth is complete — remove the stored credentials.
    if (!m_credKey.isEmpty()) {
        runHidden("cmdkey.exe", { QString("/delete:%1").arg(m_credKey) });
        m_credKey.clear();
    }
}

void RdpPane::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_mstscHwnd) return;
    if (event->size().width() <= 0 || event->size().height() <= 0) return;

    // Resize the embedded HWND immediately so content fills the tab during
    // a drag, then reconnect at the new resolution 500 ms after the user
    // stops resizing so mstsc gets the correct /w /h for the session.
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
    m_mstscHwnd = 0;
    m_cachedPass.clear(); // Require credential prompt on next manual connect
    m_status->setText(QString("Disconnected from %1.").arg(m_host));
    m_status->show();
}

void RdpPane::disconnectRdp()
{
    if (m_pollTimer) m_pollTimer->stop();
    if (m_resizeTimer) m_resizeTimer->stop();

    if (!m_credKey.isEmpty()) {
        runHidden("cmdkey.exe", { QString("/delete:%1").arg(m_credKey) });
        m_credKey.clear();
    }

    if (m_mstscHwnd) {
        HWND hwnd = reinterpret_cast<HWND>(m_mstscHwnd);
        // Restore popup style and un-parent BEFORE notifying mstsc to close,
        // so our widget's HWND has no foreign children when Qt tears it down.
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        style &= ~WS_CHILD;
        style |= WS_POPUP;
        SetWindowLong(hwnd, GWL_STYLE, style);
        SetParent(hwnd, nullptr);
        m_mstscHwnd = 0;
        // PostMessage is async — avoids a re-entrant Win32 message pump that
        // could let Qt process events while we are mid-cleanup.
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }

    if (m_process) {
        m_process->disconnect();
        if (m_process->state() != QProcess::NotRunning)
            m_process->kill();
    }
}
