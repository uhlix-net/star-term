#pragma once
#include <QWidget>
#include <QString>
#include <QJsonObject>

class QCheckBox;
class QPushButton;
class QLabel;
class SSHSession;
class TerminalWidget;
class CwdTracker;

// Matches session_pane.py exactly.
// Note: RemoteStatsWorker is not included (libssh2 doesn't have exec_command
// equivalent easily; stats poll is omitted from the C++ port for simplicity).

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

    QString     name;
    SSHSession *session     = nullptr;
    QJsonObject connectionParams;
    CwdTracker *cwdTracker  = nullptr;

    TerminalWidget *terminal       = nullptr;
    QCheckBox      *excludeCheckbox = nullptr;
    QPushButton    *reconnectBtn   = nullptr;

signals:
    void dataToSend(const QByteArray &data);
    void sizeChanged(int cols, int rows);
    void closeRequested();
    void reconnectRequested();
};
