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
    QString     name;
    QJsonObject lastStats;

    explicit RdpPane(const QJsonObject &session, QWidget *parent = nullptr);
    ~RdpPane();
    void disconnectRdp();
    void reconnect();
    // Called by the WinEvent hook callback — must be public.
    void embedWindow(WId hwnd);

signals:
    void closeRequested();
    void statsReady(const QJsonObject &stats);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void connectToHost();
    void pollForWindow();
    void onProcessFinished();
    void pollStats();

private:
    void startStatsPolling();
    void stopStatsPolling();

    QLabel         *m_status          = nullptr;
    QTimer         *m_pollTimer       = nullptr;
    QTimer         *m_resizeTimer     = nullptr;
    QTimer         *m_statsTimer      = nullptr;
    QProcess       *m_process         = nullptr;
    QProcess       *m_statsProcess    = nullptr;
    QString         m_host;
    QString         m_user;
    int             m_port            = 3389;
    QString         m_credKey;
    QString         m_cachedPass;
    QString         m_statsHost;
    QString         m_statsUser;
    QString         m_statsPass;
    WId             m_mstscHwnd       = 0;
    quintptr        m_winEventHook    = 0;
    QSet<quintptr>  m_existingWindows;
    bool            m_initialized     = false;
};
