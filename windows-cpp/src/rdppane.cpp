#include "rdppane.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

// ---------------------------------------------------------------------------
// mstsc.exe creates its RDP session container window with the class name
// "TscShellContainerClass".  This window exists from first connection through
// disconnect and is the definitive handle to embed.  The credential dialog
// that mstsc shows BEFORE connecting is a separate top-level window that we
// deliberately skip — it will float as a normal dialog — and we wait until
// the session container itself appears.
// ---------------------------------------------------------------------------

struct RdpEnumData {
    const QSet<quintptr> *exclude; // windows that existed before our launch
    HWND                  found;
};

static BOOL CALLBACK findRdpSession(HWND hwnd, LPARAM lp)
{
    auto *d = reinterpret_cast<RdpEnumData *>(lp);
    if (d->exclude->contains(reinterpret_cast<quintptr>(hwnd))) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;

    WCHAR cls[256] = {};
    GetClassNameW(hwnd, cls, 256);
    if (_wcsicmp(cls, L"TscShellContainerClass") == 0) {
        d->found = hwnd;
        return FALSE;
    }
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

RdpPane::RdpPane(const QJsonObject &session, QWidget *parent)
    : QWidget(parent)
{
    name   = session.value("name").toString();
    m_host = session.value("host").toString();
    int     port = session.value("port").toInt(3389);
    QString user = session.value("username").toString();

    // Must have a native HWND so we can use it as the Win32 parent.
    setAttribute(Qt::WA_NativeWindow);

    m_status = new QLabel(
        QString("Connecting to %1...\n\nA credential dialog may appear separately.\n"
                "After signing in, the session will open here.").arg(m_host));
    m_status->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_status);

    // Snapshot every TscShellContainerClass window that currently exists so
    // we don't accidentally steal a session the user had open before this launch.
    EnumWindows(snapshotRdpSessions,
                reinterpret_cast<LPARAM>(&m_existingWindows));

    // Write a temporary .rdp file so we can pass all parameters to mstsc.exe.
    m_tmpRdpPath = QDir::temp().filePath(
        QString("starterm_%1.rdp").arg(QDateTime::currentMSecsSinceEpoch()));

    QString rdpContent = QString(
        "full address:s:%1:%2\r\n"
        "username:s:%3\r\n"
        "prompt for credentials:i:1\r\n"
    ).arg(m_host).arg(port).arg(user);

    QFile f(m_tmpRdpPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_status->setText("Could not write temporary RDP file.");
        return;
    }
    f.write(rdpContent.toUtf8());
    f.close();

    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished,
            this, &RdpPane::onProcessFinished);

    m_process->start("mstsc.exe", { m_tmpRdpPath });
    if (!m_process->waitForStarted(5000)) {
        m_status->setText("Failed to launch mstsc.exe.");
        return;
    }

    // Poll for the TscShellContainerClass session window.  The credential
    // dialog may appear first as a separate floating window and take time to
    // complete, so we poll for up to 5 minutes before giving up.
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &RdpPane::pollForWindow);
    m_pollTimer->start(500);
    QTimer::singleShot(300000, m_pollTimer, &QTimer::stop);
}

RdpPane::~RdpPane()
{
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

    // Strip decorations so it looks embedded.
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
    m_mstscHwnd = 0;
    m_status->setText(QString("Disconnected from %1.").arg(m_host));
    m_status->show();
    QFile::remove(m_tmpRdpPath);
    m_tmpRdpPath.clear();
}

void RdpPane::disconnectRdp()
{
    if (m_pollTimer) m_pollTimer->stop();
    if (m_mstscHwnd) {
        SendMessage(reinterpret_cast<HWND>(m_mstscHwnd), WM_CLOSE, 0, 0);
        m_mstscHwnd = 0;
    }
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        m_process->waitForFinished(2000);
        m_process->kill();
    }
}
