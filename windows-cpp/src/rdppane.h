#pragma once
#include <QJsonObject>
#include <QWidget>

class QAxObject;
class QAxWidget;
class QLabel;
class QProcess;
class QTimer;
class QVBoxLayout;

// Embedded RDP session hosted via the Microsoft Terminal Services ActiveX
// control (MsRdpClient, mstscax.dll) inside an ActiveQt QAxWidget.  The control
// is a real child widget, so no HWND reparenting, WinEvent hooks or cmdkey
// credential staging is needed — credentials go straight into the control and
// the session resizes in place via UpdateSessionDisplaySettings.
class RdpPane : public QWidget {
    Q_OBJECT
public:
    QString     name;
    QJsonObject lastStats;

    explicit RdpPane(const QJsonObject &session, QWidget *parent = nullptr);
    ~RdpPane();
    void disconnectRdp();
    void reconnect();

signals:
    void closeRequested();
    void statsReady(const QJsonObject &stats);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    // Qt consumes Tab for focus navigation before it can reach the control;
    // refuse the navigation so Tab is delivered to the remote session.
    bool focusNextPrevChild(bool next) override;

private slots:
    void connectToHost();
    void onAxConnected();
    void onAxLoginComplete();
    void onAxDisconnected(int discReason);
    void onAxLogonError(int lError);
    void onAxFatalError(int errorCode);
    void retryAfterCredentialFailure();
    void applyPendingResize();
    void pollStats();

private:
    bool        createControl();
    void        destroyControl();
    QAxObject  *advancedSettings();
    QSize       sessionPixelSize() const;
    QString     disconnectText(int discReason);
    QString     credentialsRefusedText() const;
    bool        promptForCredentials();
    void        showStatus(const QString &text);
    void        startStatsPolling();
    void        stopStatsPolling();

    QAxWidget   *m_ax            = nullptr;
    QAxObject   *m_advanced      = nullptr;   // parented to m_ax
    QLabel      *m_status        = nullptr;
    QVBoxLayout *m_layout        = nullptr;
    QTimer      *m_resizeTimer   = nullptr;
    QTimer      *m_statsTimer    = nullptr;
    QProcess    *m_statsProcess  = nullptr;
    QString      m_host;
    QString      m_user;
    QString      m_domain;
    int          m_port          = 3389;
    QString      m_cachedPass;
    QString      m_statsHost;
    QString      m_statsUser;
    QString      m_statsPass;
    bool         m_initialized   = false;
    bool         m_userClosing   = false;
    // Whether the session ever got past logon, and whether a logon was refused:
    // together they separate a credential problem from an ordinary disconnect.
    bool         m_loggedIn      = false;
    bool         m_logonFailed   = false;
};
