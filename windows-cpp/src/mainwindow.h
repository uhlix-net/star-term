#pragma once
#include <QFile>
#include <QHash>
#include <QMainWindow>
#include <QList>
#include <QJsonObject>

class QAction;
class QActionGroup;
class QGridLayout;
class QSplitter;
class QStackedWidget;
class QTabWidget;

class MacrosPanel;
class RdpPane;
class RemoteFileBrowser;
class SessionPane;
class SessionSidebar;
class SystemStatusBar;
class UpdateChecker;

// Matches main.py MainWindow exactly.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

public slots:
    void openConnectionDialog();
    void openPreferencesDialog();
    void disconnectSession();
    void showAboutDialog();
    void showUpdatesDialog();
    void showLicenseDialog();
    void exportSessions();
    void importSessions();
    void toggleMultiExecView(bool checked);
    void showError(const QString &message);
    void checkForUpdates();

    void connectSession(const QJsonObject &params, const QString &name = QString());
    void connectSavedSession(const QJsonObject &session);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onTabCloseRequested(int index);
    void onTabChanged(int index);
    void onHostKeyUnknown(const QString &host, const QString &keyType,
                          const QString &fingerprint, const QString &hexHash);

private:
    void addPane(SessionPane *pane);
    void closePane(SessionPane *pane);
    void closeRdpPane(RdpPane *pane);
    void startSession(SessionPane *pane, const QJsonObject &params);
    void reconnectPane(SessionPane *pane);
    void onDataToSend(SessionPane *pane, const QByteArray &data);
    void onSizeChanged(SessionPane *pane, int cols, int rows);
    void onSessionEnded(SessionPane *pane);
    void onSessionConnected(SessionPane *pane);
    void runMacro(const QString &commands, bool autoExecute);
    void populateMultiExecGrid();
    void populateTabs();
    void resizeForMultiExec();
    void startPaneLogging(SessionPane *pane);
    void stopPaneLogging(SessionPane *pane);
    void toggleSessionLogging(bool enabled);

    QList<SessionPane*>  m_panes;
    SessionSidebar      *m_sidebar          = nullptr;
    UpdateChecker       *m_updateChecker    = nullptr;
    QTabWidget          *m_tabs             = nullptr;
    QWidget             *m_multiexecContainer = nullptr;
    QGridLayout         *m_multiexecLayout  = nullptr;
    QStackedWidget      *m_viewStack        = nullptr;
    RemoteFileBrowser   *m_remoteBrowser    = nullptr;
    MacrosPanel         *m_macrosPanel      = nullptr;
    QStackedWidget      *m_leftStack        = nullptr;
    QSplitter           *m_splitter         = nullptr;
    SystemStatusBar     *m_statusBar        = nullptr;

    QHash<SessionPane*, QFile*> m_sessionLogs;

    QAction *m_connectAction        = nullptr;
    QAction *m_closeSessionAction   = nullptr;
    QAction *m_multiExecAction      = nullptr;
    QAction *m_preferencesAction    = nullptr;
    QAction *m_sessionsAction       = nullptr;
    QAction *m_remoteFilesAction    = nullptr;
    QAction *m_macrosAction         = nullptr;
    QAction *m_toggleSidebarAction  = nullptr;
    QAction *m_sessionLoggingAction = nullptr;
};
