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
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void connectToHost();
    void pollForWindow();
    void onProcessFinished();

private:
    QLabel         *m_status          = nullptr;
    QTimer         *m_pollTimer       = nullptr;
    QProcess       *m_process         = nullptr;
    QString         m_host;
    QString         m_user;
    int             m_port            = 3389;
    QString         m_credKey;
    WId             m_mstscHwnd       = 0;
    QSet<quintptr>  m_existingWindows;
    bool            m_initialized     = false;
};
