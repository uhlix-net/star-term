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

// ----------------------------------------------------------------------------
// Win32 window-search helper — finds the first visible top-level window owned
// by a specific PID.  Used to locate the mstsc.exe window after launch.
// ----------------------------------------------------------------------------
struct FindWinData { DWORD pid; HWND hwnd; };

static BOOL CALLBACK enumProc(HWND hwnd, LPARAM lp)
{
    auto *d = reinterpret_cast<FindWinData *>(lp);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == d->pid && IsWindowVisible(hwnd) && GetParent(hwnd) == nullptr) {
        d->hwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

// ----------------------------------------------------------------------------

RdpPane::RdpPane(const QJsonObject &session, QWidget *parent)
    : QWidget(parent)
{
    name   = session.value("name").toString();
    m_host = session.value("host").toString();
    int     port = session.value("port").toInt(3389);
    QString user = session.value("username").toString();

    // Must have a native HWND so we can use it as the Win32 parent.
    setAttribute(Qt::WA_NativeWindow);

    m_status = new QLabel(QString("Connecting to %1…").arg(m_host));
    m_status->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_status);

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

    // Poll until mstsc's window becomes visible, then embed it.
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &RdpPane::pollForWindow);
    m_pollTimer->start(200);
    // Give up after 20 seconds if no window appears.
    QTimer::singleShot(20000, m_pollTimer, &QTimer::stop);
}

RdpPane::~RdpPane()
{
    QFile::remove(m_tmpRdpPath);
}

void RdpPane::pollForWindow()
{
    if (!m_process) return;

    DWORD pid = static_cast<DWORD>(m_process->processId());
    FindWinData data = { pid, nullptr };
    EnumWindows(enumProc, reinterpret_cast<LPARAM>(&data));
    if (!data.hwnd) return;

    m_pollTimer->stop();

    HWND hwnd = data.hwnd;
    HWND ours = reinterpret_cast<HWND>(winId());

    // Strip window decorations so it looks embedded, not floating.
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
               WS_MAXIMIZEBOX | WS_SYSMENU | WS_BORDER);
    style |= WS_CHILD;
    SetWindowLong(hwnd, GWL_STYLE, style);

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                 WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

    // Reparent into our widget.
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
        SetWindowPos(hwnd, nullptr, 0, 0, event->size().width(), event->size().height(),
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
