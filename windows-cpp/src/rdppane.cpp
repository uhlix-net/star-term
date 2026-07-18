#include "rdppane.h"

#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

// ---------------------------------------------------------------------------
// EnumWindows callbacks — find TscShellContainerClass windows (mstsc's RDP
// session container, distinct from the pre-connection credential dialog).
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
// Run a console utility (e.g. cmdkey.exe) without flashing a console window.
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
    int  port = session.value("port").toInt(3389);
    m_user    = session.value("username").toString();

    setAttribute(Qt::WA_NativeWindow);

    m_status = new QLabel(QString("Connecting to %1...").arg(m_host));
    m_status->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_status);

    // --- Collect credentials inside the app before launching mstsc ----------
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
    m_user        = userEdit->text();
    QString pass  = passEdit->text();

    // Store credentials temporarily in Windows Credential Manager so mstsc
    // picks them up automatically and skips its own credential prompt.
    m_credKey = QString("TERMSRV/%1").arg(m_host);
    runHidden("cmdkey.exe", {
        QString("/generic:%1").arg(m_credKey),
        QString("/user:%1").arg(m_user),
        QString("/pass:%1").arg(pass)
    });

    // Snapshot existing TscShellContainerClass windows before launching.
    EnumWindows(snapshotRdpSessions, reinterpret_cast<LPARAM>(&m_existingWindows));

    // Write temp .rdp file.
    m_tmpRdpPath = QDir::temp().filePath(
        QString("starterm_%1.rdp").arg(QDateTime::currentMSecsSinceEpoch()));

    QString rdpContent = QString(
        "full address:s:%1:%2\r\n"
        "username:s:%3\r\n"
        "prompt for credentials:i:0\r\n"
        "authentication level:i:2\r\n"
    ).arg(m_host).arg(port).arg(m_user);

    QFile f(m_tmpRdpPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_status->setText("Could not write temporary RDP file.");
        return;
    }
    f.write(rdpContent.toUtf8());
    f.close();

    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &RdpPane::onProcessFinished);
    m_process->start("mstsc.exe", { m_tmpRdpPath });
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
    // CRITICAL: un-parent the mstsc window before Qt destroys our native HWND.
    // While the mstsc window is a Win32 child of our HWND, Win32 sends
    // WM_DESTROY to it when our HWND is torn down — mstsc's window proc
    // handles that unexpectedly and causes an access violation in Qt6Core.dll.
    if (m_mstscHwnd) {
        HWND hwnd = reinterpret_cast<HWND>(m_mstscHwnd);
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        style &= ~WS_CHILD;
        style |= WS_POPUP;
        SetWindowLong(hwnd, GWL_STYLE, style);
        SetParent(hwnd, nullptr);
        m_mstscHwnd = 0;
    }
    if (!m_credKey.isEmpty()) {
        runHidden("cmdkey.exe", { QString("/delete:%1").arg(m_credKey) });
        m_credKey.clear();
    }
    QFile::remove(m_tmpRdpPath);
}

void RdpPane::pollForWindow()
{
    RdpEnumData data { &m_existingWindows, nullptr };
    EnumWindows(findRdpSession, reinterpret_cast<LPARAM>(&data));
    if (!data.found) return;

    m_pollTimer->stop();

    HWND hwnd = data.found;
    HWND ours = reinterpret_cast<HWND>(winId());

    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
               WS_MAXIMIZEBOX | WS_SYSMENU | WS_BORDER);
    style |= WS_CHILD;
    SetWindowLong(hwnd, GWL_STYLE, style);

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                 WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

    SetParent(hwnd, ours);
    m_mstscHwnd = reinterpret_cast<WId>(hwnd);

    SetWindowPos(hwnd, HWND_TOP, 0, 0, width(), height(),
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    m_status->hide();

    // Credentials were consumed during NLA authentication — clean up now.
    if (!m_credKey.isEmpty()) {
        runHidden("cmdkey.exe", { QString("/delete:%1").arg(m_credKey) });
        m_credKey.clear();
    }
}

void RdpPane::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_mstscHwnd) {
        HWND hwnd = reinterpret_cast<HWND>(m_mstscHwnd);
        SetWindowPos(hwnd, nullptr, 0, 0,
                     event->size().width(), event->size().height(),
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void RdpPane::onProcessFinished()
{
    if (m_pollTimer) m_pollTimer->stop();
    m_mstscHwnd = 0; // Window already destroyed by the exiting process
    m_status->setText(QString("Disconnected from %1.").arg(m_host));
    m_status->show();
    QFile::remove(m_tmpRdpPath);
    m_tmpRdpPath.clear();
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
        // Restore popup style and unparent before closing so Win32 doesn't
        // send WM_DESTROY through our widget's HWND on process termination.
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        style &= ~WS_CHILD;
        style |= WS_POPUP;
        SetWindowLong(hwnd, GWL_STYLE, style);
        SetParent(hwnd, nullptr);
        m_mstscHwnd = 0;
        SendMessage(hwnd, WM_CLOSE, 0, 0);
    }

    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        m_process->waitForFinished(2000);
        m_process->kill();
    }
}
