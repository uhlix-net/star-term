#pragma once
#include <QJsonObject>
#include <QWidget>

class QAxWidget;
class QLabel;

// Hosts the Windows RDP ActiveX control (mstscax.dll / MsRdpClient10)
// embedded in a tab, replacing the external mstsc.exe launch.
class RdpPane : public QWidget {
    Q_OBJECT
public:
    QString name;
    explicit RdpPane(const QJsonObject &session, QWidget *parent = nullptr);
    void disconnectRdp();

signals:
    void closeRequested();

private slots:
    void onConnected();
    void onDisconnected(long reason);

private:
    QAxWidget *m_rdp    = nullptr;
    QLabel    *m_status = nullptr;
    QString    m_host;
};
