#pragma once
#include <QJsonObject>
#include <QShowEvent>
#include <QWidget>

class QAxWidget;
class QLabel;

// Hosts the Windows RDP ActiveX control (mstscax.dll / MsRdpClient10)
// embedded in a tab. ActiveX initialization is deferred to showEvent so
// the widget has a native Win32 HWND before setControl() is called.
class RdpPane : public QWidget {
    Q_OBJECT
public:
    QString name;
    explicit RdpPane(const QJsonObject &session, QWidget *parent = nullptr);
    void disconnectRdp();

signals:
    void closeRequested();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void initRdp();
    void onConnected();
    void onDisconnected(long reason);

private:
    QAxWidget *m_rdp         = nullptr;
    QLabel    *m_status      = nullptr;
    QString    m_host;
    QString    m_user;
    int        m_port        = 3389;
    bool       m_initialized = false;
};
