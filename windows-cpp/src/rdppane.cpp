#include "rdppane.h"
#include "config.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QTextStream>
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

    // Scroll-mode container — hidden until a scroll-mode session connects.
    m_scrollContainer = new QWidget();
    m_scrollContainer->setAttribute(Qt::WA_NativeWindow);
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidget(m_scrollContainer);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_scrollArea->hide();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_status);
    layout->addWidget(m_scrollArea, 1);
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
    // Read the resize mode preference set in Preferences -> RDP.
    QJsonObject settings = loadSettings();
    m_scaleMode = (settings.value("rdp_resize_mode").toString("scroll") == "scale");

    // Clean up any previous timer/process from a prior connection attempt.
    if (m_pollTimer) { m_pollTimer->deleteLater(); m_pollTimer = nullptr; }
    if (m_process)   { m_process->deleteLater();   m_process   = nullptr; }

    // Credential dialog — skipped when credentials are cached (automatic
    // reconnect triggered by a settings change).  Always shown on first
    // connect and after the session has been closed by the user.
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

    // Session resolution in physical pixels so the remote desktop exactly fills
    // the HWND — mstsc renders its content at the pixel dimensions passed via
    // /w and /h, so using physical (not logical) pixels avoids blank strips
    // when the HWND is larger than the session resolution.
    const qreal dpr = devicePixelRatioF();
    m_sessionPw = qRound(width()  * dpr);
    m_sessionPh = qRound(height() * dpr);

    // Scroll mode: write a minimal .rdp file that sets desktop scale factor to
    // 100% so mstsc reports 96 DPI to the remote server, giving 1:1 pixel
    // mapping.  The file intentionally omits "full address" — host comes from
    // the command-line /v flag — so mstsc never routes through an existing
    // instance and no unsigned-file dialog is triggered.
    if (!m_scaleMode) {
        m_rdpFilePath = QDir::tempPath() +
                        QString("/star_term_rdp_%1.rdp").arg(reinterpret_cast<quintptr>(this));
        QFile f(m_rdpFilePath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << "desktop scale factor:i:100\r\n"
               << "device scale factor:i:100\r\n";
        } else {
            m_rdpFilePath.clear();
        }
    }

    QStringList args;
    if (!m_rdpFilePath.isEmpty())
        args << m_rdpFilePath;
    args << QString("/v:%1:%2").arg(m_host).arg(m_port);
    if (m_sessionPw > 0 && m_sessionPh > 0)
        args << QString("/w:%1").arg(m_sessionPw) << QString("/h:%1").arg(m_sessionPh);

    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &RdpPane::onProcessFinished);
    m_process->start("mstsc.exe", args);

    if (!m_process->waitForStarted(5000)) {
        m_status->setText("Failed to launch mstsc.exe.");
        return;
    }

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &RdpPane::pollForWindow);
    m_pollTimer->start(100);   // 100 ms — faster detection reduces any residual flash
    QTimer::singleShot(300000, m_pollTimer, &QTimer::stop);
}

void RdpPane::reconnect()
{
    disconnectRdp();

    // Reset UI so the status label is visible while reconnecting.
    m_scrollArea->hide();
    m_status->setText(QString("Reconnecting to %1...").arg(m_host));
    m_status->show();

    // Cached credentials are reused — the credential dialog will not appear.
    QTimer::singleShot(0, this, &RdpPane::connectToHost);
}

RdpPane::~RdpPane()
{
    // If disconnectRdp() was already called (normal close path), m_mstscHwnd
    // is 0 and m_process is disconnected — these are just safety nets.
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
    if (!m_rdpFilePath.isEmpty())
        QFile::remove(m_rdpFilePath);
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

    if (m_scaleMode) {
        // Scale mode: embed directly in the RdpPane.  resizeEvent keeps the
        // mstsc HWND sized to the tab (capped at original session resolution).
        HWND ours = reinterpret_cast<HWND>(winId());
        SetParent(hwnd, ours);
        m_mstscHwnd = reinterpret_cast<WId>(hwnd);

        const qreal dpr = devicePixelRatioF();
        SetWindowPos(hwnd, HWND_TOP, 0, 0,
                     qRound(width() * dpr), qRound(height() * dpr),
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        m_status->hide();
    } else {
        // Scroll mode: embed inside the scroll container at the fixed session
        // resolution.  The QScrollArea provides Qt scroll bars when the tab
        // is smaller than the session; no blank area when it is larger.
        // m_sessionPw/Ph are physical pixels; the container uses logical pixels.
        const qreal dpr = devicePixelRatioF();
        m_scrollContainer->resize(qRound(m_sessionPw / dpr), qRound(m_sessionPh / dpr));

        m_scrollArea->show();
        m_status->hide();

        // Showing the scroll area ensures the container's native HWND exists.
        HWND containerHwnd = reinterpret_cast<HWND>(m_scrollContainer->winId());
        SetParent(hwnd, containerHwnd);
        m_mstscHwnd = reinterpret_cast<WId>(hwnd);

        SetWindowPos(hwnd, HWND_TOP, 0, 0,
                     m_sessionPw, m_sessionPh,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }

    // NLA auth is complete — remove the stored credentials.
    if (!m_credKey.isEmpty()) {
        runHidden("cmdkey.exe", { QString("/delete:%1").arg(m_credKey) });
        m_credKey.clear();
    }
}

void RdpPane::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_mstscHwnd || !m_scaleMode) return;
    if (event->size().width() <= 0 || event->size().height() <= 0) return;

    // Scale mode: resize the mstsc HWND to follow the tab, capped at the
    // original session resolution to prevent mstsc from going blank when the
    // window is stretched beyond the session size.
    // m_sessionPw/Ph are already physical pixels; use them directly as the cap.
    HWND hwnd = reinterpret_cast<HWND>(m_mstscHwnd);
    const qreal dpr = devicePixelRatioF();
    int newW = qMin(qRound(event->size().width()  * dpr), m_sessionPw);
    int newH = qMin(qRound(event->size().height() * dpr), m_sessionPh);
    SetWindowPos(hwnd, nullptr, 0, 0, newW, newH, SWP_NOZORDER | SWP_NOACTIVATE);
}

void RdpPane::onProcessFinished()
{
    if (m_pollTimer) m_pollTimer->stop();
    m_mstscHwnd = 0; // Window already gone — process exited
    m_cachedPass.clear(); // Require credential prompt on next manual connect
    if (!m_rdpFilePath.isEmpty()) {
        QFile::remove(m_rdpFilePath);
        m_rdpFilePath.clear();
    }
    m_status->setText(QString("Disconnected from %1.").arg(m_host));
    m_status->show();
    m_scrollArea->hide();
}

void RdpPane::disconnectRdp()
{
    if (m_pollTimer) m_pollTimer->stop();

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
        m_process->disconnect(); // Prevent onProcessFinished after cleanup
        if (m_process->state() != QProcess::NotRunning)
            m_process->kill();   // No blocking waitForFinished
    }
    if (!m_rdpFilePath.isEmpty()) {
        QFile::remove(m_rdpFilePath);
        m_rdpFilePath.clear();
    }
}
