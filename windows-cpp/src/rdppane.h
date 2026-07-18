#pragma once
#include <QJsonObject>
#include <QProcess>
#include <QWidget>

class QLabel;
class QTimer;

// Launches mstsc.exe with a temporary .rdp file, then locates its top-level
// window by PID and reparents it into this widget using Win32 SetParent.
// This avoids the ActiveX / AxContainer path entirely.
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
    QLabel   *m_status     = nullptr;
    QTimer   *m_pollTimer  = nullptr;
    QProcess *m_process    = nullptr;
    QString   m_host;
    QString   m_tmpRdpPath;
    WId       m_mstscHwnd  = 0;  // WId = quintptr = same size as HWND on Windows
};
