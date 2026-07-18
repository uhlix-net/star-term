#pragma once
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QWidget>

class QLabel;
class QTimer;

// Launches mstsc.exe with a temporary .rdp file, then locates its top-level
// window by process name (not PID — on Windows 10/11 the session window can
// live in a child/surrogate process) and reparents it via Win32 SetParent.
// A pre-launch snapshot of existing mstsc windows prevents stealing sessions
// that were already open.
class RdpPane : public QWidget {
    Q_OBJECT
public:
    QString name;
    explicit RdpPane(const QJsonObject &session, QWidget *parent = nullptr);
    ~RdpPane();
    void disconnectRdp();

signals:
    void closeRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void pollForWindow();
    void onProcessFinished();

private:
    QLabel          *m_status          = nullptr;
    QTimer          *m_pollTimer       = nullptr;
    QProcess        *m_process         = nullptr;
    QString          m_host;
    QString          m_tmpRdpPath;
    WId              m_mstscHwnd       = 0;
    QSet<quintptr>   m_existingWindows; // mstsc windows that existed before we launched
};
