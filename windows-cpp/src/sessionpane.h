#pragma once
#include <QWidget>
#include <QString>
#include <QJsonObject>

class QCheckBox;
class QPushButton;
class QLabel;
class RemoteStatsWorker;
class TerminalSession;
class TerminalWidget;
class CwdTracker;

class SessionPane : public QWidget {
    Q_OBJECT
public:
    explicit SessionPane(
        const QString &name,
        const QString &fontFamily = "Courier New",
        int fontSize = 10,
        const QString &cursorStyle = "underline",
        QWidget *parent = nullptr);

    ~SessionPane() override;

    void applySettings(const QString &fontFamily, int fontSize, const QString &cursorStyle);
    void disconnectSession();
    void startStatsWorker();
    void stopStatsWorker();

    // The control strip below the terminal is shown only when it has something
    // to offer, so an ordinary connected session is pure terminal.
    void setReconnectVisible(bool on);
    void setMultiExecControlsVisible(bool on);

    QString          name;
    // SSH or WSL; qobject_cast to SSHSession for the SSH-only facilities.
    TerminalSession *session     = nullptr;
    QJsonObject connectionParams;
    QJsonObject lastStats;          // last received stats; empty = not yet available
    CwdTracker *cwdTracker       = nullptr;

    TerminalWidget *terminal        = nullptr;
    QCheckBox      *excludeCheckbox = nullptr;
    QPushButton    *reconnectBtn    = nullptr;

signals:
    void dataToSend(const QByteArray &data);
    void sizeChanged(int cols, int rows);
    void closeRequested();
    void reconnectRequested();
    void statsUpdated(const QJsonObject &stats);

private:
    void updateControlsVisibility();

    RemoteStatsWorker *m_statsWorker = nullptr;
    QWidget           *m_controls    = nullptr;
};
