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
#include <QStandardPaths>
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

    // Scroll-mode widgets — hidden until a scroll-mode session is established.
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
    // Read the resize mode preference set in Preferences → RDP.
    QJsonObject settings = loadSettings();
    m_scaleMode = (settings.value("rdp_resize_mode").toString("scroll") == "scale");

    // --- Credential dialog (appears inside the app, not as a separate window) ---
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
    QString pass = passEdit->text();

    // Store credentials temporarily in Windows Credential Manager so mstsc
    // authenticates silently via NLA without showing its own credential dialog.
    m_credKey = QString("TERMSRV/%1").arg(m_host);
    runHidden("cmdkey.exe", {
        QString("/generic:%1").arg(m_credKey),
        QString("/user:%1").arg(m_user),
        QString("/pass:%1").arg(pass)
    });

    // Snapshot existing TscShellContainerClass windows so we don't steal one.
    EnumWindows(snapshotRdpSessions, reinterpret_cast<LPARAM>(&m_existingWindows));

    // Physical-pixel dimensions for the session resolution and Win32 calls.
    const qreal dpr = devicePixelRatioF();
    m_sessionPw = qRound(width()  * dpr);
    m_sessionPh = qRound(height() * dpr);

    QStringList args;
    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &RdpPane::onProcessFinished);

    if (m_scaleMode) {
        // Scale mode: write a minimal .rdp file with smart sizing enabled.
        // mstsc will scale the session content to fill the window when it is
        // resized via SetWindowPos.  This requires a .rdp file — command-line
        // flags alone cannot enable smart sizing.  Windows will show a one-time
        // unsigned-file security prompt before the connection is made.
        QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        m_rdpFilePath = tmpDir + QString("/star_term_rdp_%1.rdp").arg(
            reinterpret_cast<quintptr>(this));

        QFile f(m_rdpFilePath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << "screen mode id:i:1\r\n"
               << QString("desktopwidth:i:%1\r\n").arg(m_sessionPw)
               << QString("desktopheight:i:%1\r\n").arg(m_sessionPh)
               << "smart sizing:i:1\r\n"
               << "prompt for credentials:i:0\r\n"
               << "authentication level:i:2\r\n"
               << QString("full address:s:%1:%2\r\n").arg(m_host).arg(m_port)
               << QString("username:s:%1\r\n").arg(m_user);
        }
        args << m_rdpFilePath;
    } else {
        // Scroll mode: command-line only — avoids the unsigned-file dialog.
        args << QString("/v:%1:%2").arg(m_host).arg(m_port);
        if (m_sessionPw > 0 && m_sessionPh > 0)
            args << QString("/w:%1").arg(m_sessionPw) << QString("/h:%1").arg(m_sessionPh);
    }

    m_process->start("mstsc.exe", args);
    if (!m_process->waitForStarted(5000)) {
        m_status->setText("Failed to launch mstsc.exe.");
        return;
    }

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &RdpPane::pollForWindow);
    m_pollTimer->start(500);
    QTimer::singleShot(300000, m_pollTimer, &QTimer::stop);
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
        // Scale mode: embed directly in the RdpPane.  resizeEvent will call
        // SetWindowPos to keep mstsc filling the tab; smart sizing scales
        // the content to fit.
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
        // resolution.  The QScrollArea provides scrollbars when the tab is
        // smaller than the session; no blank area when it is larger.
        const qreal dpr = devicePixelRatioF();
        const int lw = qRound(m_sessionPw / dpr);
        const int lh = qRound(m_sessionPh / dpr);
        m_scrollContainer->resize(lw, lh);

        m_scrollArea->show();
        m_status->hide();

        // Showing the scroll area ensures its viewport and container have
        // native HWNDs before we call winId().
        HWND containerHwnd = reinterpret_cast<HWND>(m_scrollContainer->winId());
        SetParent(hwnd, containerHwnd);
        m_mstscHwnd = reinterpret_cast<WId>(hwnd);

        SetWindowPos(hwnd, HWND_TOP, 0, 0,
                     m_sessionPw, m_sessionPh,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }

    // NLA auth is complete at this point — remove the stored credentials.
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

    // Scale mode: resize the mstsc HWND to fill the tab.  Smart sizing
    // (enabled via the .rdp file) scales the session content to fit.
    HWND hwnd = reinterpret_cast<HWND>(m_mstscHwnd);
    const qreal dpr = devicePixelRatioF();
    SetWindowPos(hwnd, nullptr, 0, 0,
                 qRound(event->size().width()  * dpr),
                 qRound(event->size().height() * dpr),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void RdpPane::onProcessFinished()
{
    if (m_pollTimer) m_pollTimer->stop();
    m_mstscHwnd = 0; // Window already gone — process exited
    m_status->setText(QString("Disconnected from %1.").arg(m_host));
    m_status->show();
    m_scrollArea->hide();
    if (!m_rdpFilePath.isEmpty()) {
        QFile::remove(m_rdpFilePath);
        m_rdpFilePath.clear();
    }
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
