#pragma once
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QWidget>

class QLabel;
class QTimer;

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
    QLabel         *m_status          = nullptr;
    QTimer         *m_pollTimer       = nullptr;
    QProcess       *m_process         = nullptr;
    QString         m_host;
    QString         m_user;
    QString         m_tmpRdpPath;
    QString         m_credKey;         // TERMSRV/<host> entry stored in Credential Manager
    WId             m_mstscHwnd       = 0;
    QSet<quintptr>  m_existingWindows;
};
