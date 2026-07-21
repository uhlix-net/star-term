#pragma once
#include <QWidget>
#include <QListWidget>
#include <QThread>
#include <QString>
#include <QObject>

#include <libssh2.h>
#include <libssh2_sftp.h>

class QLabel;
class QLineEdit;
class QMutex;
class QPushButton;
class QToolButton;
class QProgressBar;
class SessionPane;

// -----------------------------------------------------------------------
// CwdTracker — infers remote cwd from typed cd commands (matches Python)
// -----------------------------------------------------------------------
class CwdTracker : public QObject {
    Q_OBJECT
public:
    explicit CwdTracker(QObject *parent = nullptr);

    void setHome(const QString &home);
    QString cwd() const { return m_cwd; }

public slots:
    void feedInput(const QByteArray &data);
    void feedServerData(const QByteArray &data);

signals:
    void cwdChanged(const QString &path);

private:
    void consumeEscape(QChar ch);
    void processLine(const QString &line);
    QString resolve(const QString &arg);

    QString m_home;
    QString m_cwd;
    QString m_buffer;
    QString m_escapeState;      // "", "esc", or "csi"
    QString m_serverEscState;   // escape state for feedServerData
    bool    m_tabPending = false;
};

// -----------------------------------------------------------------------
// SFTPWorker — runs a single SFTP op on a background thread.
// All libssh2 calls are serialized via sessionLock (shared with SSHSession).
// -----------------------------------------------------------------------
struct SFTPEntry {
    QString name;
    bool    isDir;
    qint64  size;
};

class SFTPWorker : public QThread {
    Q_OBJECT
public:
    enum Op { List, Home, Upload, Download };

    // Home
    SFTPWorker(LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftp, QMutex *sessionLock,
               Op op, QObject *parent = nullptr);
    // Upload: localPath -> remotePath
    SFTPWorker(LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftp, QMutex *sessionLock,
               const QString &localPath, const QString &remotePath,
               QObject *parent = nullptr);
    // Download: remotePath -> localPath  (bool flag disambiguates from upload)
    SFTPWorker(LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftp, QMutex *sessionLock,
               const QString &remotePath, const QString &localPath,
               bool download, QObject *parent = nullptr);
    // List directory at path
    SFTPWorker(LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftp, QMutex *sessionLock,
               const QString &path, QObject *parent = nullptr);

signals:
    void listed(const QString &path, const QList<SFTPEntry> &entries);
    void homeResolved(const QString &home);
    void transferred(const QString &mode, const QString &path);
    void progress(qint64 done, qint64 total);
    void error(const QString &msg);

protected:
    void run() override;

private:
    LIBSSH2_SESSION *m_session;
    LIBSSH2_SFTP    *m_sftp;
    QMutex          *m_sessionLock;
    Op               m_op;
    QString          m_localPath;
    QString          m_remotePath;
};

// -----------------------------------------------------------------------
// RemoteFileList — QListWidget with drag-drop (matches Python)
// -----------------------------------------------------------------------
class RemoteFileList : public QListWidget {
    Q_OBJECT
public:
    explicit RemoteFileList(QWidget *parent = nullptr);

    LIBSSH2_SESSION *session     = nullptr;
    LIBSSH2_SFTP    *sftp        = nullptr;
    QMutex          *sessionLock = nullptr;
    QString          remotePath  = "/";

signals:
    void uploadRequested(const QStringList &localPaths);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event)   override;
    void dropEvent(QDropEvent *event)           override;
    void startDrag(Qt::DropActions supportedActions) override;
};

// -----------------------------------------------------------------------
// RemoteFileBrowser — full SFTP browser panel (matches Python)
// -----------------------------------------------------------------------
class RemoteFileBrowser : public QWidget {
    Q_OBJECT
public:
    explicit RemoteFileBrowser(QWidget *parent = nullptr);

    void setPane(SessionPane *pane);

private slots:
    void onPathEntered();
    void goUp();
    void refresh();
    void onListed(const QString &path, const QList<SFTPEntry> &entries);
    void onHomeResolved(const QString &home);
    void onError(const QString &message);
    void onItemDoubleClicked(QListWidgetItem *item);
    void onContextMenu(const QPoint &pos);
    void onUploadRequested(const QStringList &localPaths);
    void onUploadDialog();
    void onDownloadDialog(const QStringList &names);
    void onDownloadProgress(qint64 done, qint64 total);
    void onDownloadFinished(const QString &mode, const QString &path);
    void onDownloadError(const QString &message);
    void onCwdChanged(const QString &path);

private:
    void resolveHome();
    void setPath(const QString &path);
    void startNextDownload();
    void runWorker(SFTPWorker *worker);

    SessionPane     *m_pane         = nullptr;
    LIBSSH2_SESSION *m_session      = nullptr;
    LIBSSH2_SFTP    *m_sftp         = nullptr;
    QMutex          *m_sessionLock  = nullptr;
    QString          m_currentPath;

    QLineEdit      *m_pathEdit      = nullptr;
    RemoteFileList *m_listWidget    = nullptr;
    QLabel         *m_statusLabel   = nullptr;
    QPushButton    *m_followBtn     = nullptr;
    QProgressBar   *m_progressBar   = nullptr;

    QList<SFTPWorker*> m_workers;

    // Download queue
    QList<QPair<QString,QString>> m_downloadQueue;
    int m_downloadTotal = 0;
    int m_downloadIndex = 0;
};
