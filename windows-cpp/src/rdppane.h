#pragma once
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QWidget>

class QLabel;
class QScrollArea;
class QTimer;

class RdpPane : public QWidget {
    Q_OBJECT
public:
    QString name;
    explicit RdpPane(const QJsonObject &session, QWidget *parent = nullptr);
    ~RdpPane();
    void disconnectRdp();
    void reconnect();

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
    QScrollArea    *m_scrollArea      = nullptr;
    QWidget        *m_scrollContainer = nullptr;
    QTimer         *m_pollTimer       = nullptr;
    QProcess       *m_process         = nullptr;
    QString         m_host;
    QString         m_user;
    int             m_port            = 3389;
    QString         m_credKey;
    QString         m_cachedPass;
    QString         m_rdpFilePath;
    WId             m_mstscHwnd       = 0;
    QSet<quintptr>  m_existingWindows;
    bool            m_initialized     = false;
    bool            m_scaleMode       = false;
    int             m_sessionPw       = 0;
    int             m_sessionPh       = 0;
};
