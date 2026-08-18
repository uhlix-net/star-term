#pragma once
#include <QWidget>
#include <QListWidget>
#include <QThread>
#include <QString>
#include <QObject>
#include <QAtomicInt>

#include <libssh2.h>
#include <libssh2_sftp.h>

class QLabel;
class QLineEdit;
class QMutex;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QListWidgetItem;
class SessionPane;

// Item data roles for the download progress rows.
enum DownloadRole {
    DownloadPercentRole = Qt::UserRole + 100,   // int 0..100
    DownloadStatusRole  = Qt::UserRole + 101    // text drawn inside the bar
};

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

    // Ask a running download to stop. Safe to call from the UI thread: the
    // transfer loop checks the flag between chunks and deletes the partial file.
    void cancel() { m_cancelled.storeRelaxed(1); }

signals:
    void listed(const QString &path, const QList<SFTPEntry> &entries);
    void homeResolved(const QString &home);
    void transferred(const QString &mode, const QString &path);
    void progress(qint64 done, qint64 total);
    void error(const QString &msg);
    // Emitted instead of transferred/error when cancel() stopped the transfer.
    // localPath has already been deleted by the time this arrives.
    void cancelled(const QString &localPath);

protected:
    void run() override;

private:
    LIBSSH2_SESSION *m_session;
    LIBSSH2_SFTP    *m_sftp;
    QMutex          *m_sessionLock;
    Op               m_op;
    QString          m_localPath;
    QString          m_remotePath;
    QAtomicInt       m_cancelled { 0 };
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
    // Windows prefix for a local (WSL) filesystem; empty means SFTP.
    QString          fsRoot;

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
    void onDownloadCancelled(const QString &localPath);
    void onCwdChanged(const QString &path);

private:
    void resolveHome();
    void setPath(const QString &path);
    void startNextDownload();
    void runWorker(SFTPWorker *worker);

    // Per-file download progress rows.
    void buildProgressRows(const QList<QPair<QString,QString>> &pairs);
    void clearProgressRows();
    void setRowState(int index, int percent, const QString &status);
    void cancelDownloads();

    // A WSL distribution is browsed through its Windows share instead of SFTP:
    // ordinary file APIs, no session, no worker threads.
    bool    fsMode() const { return !m_fsRoot.isEmpty(); }
    void    listFilesystem(const QString &path);
    QString fsPathFor(const QString &posixPath) const;
    bool    connected() const { return m_sftp != nullptr || fsMode(); }

    SessionPane     *m_pane         = nullptr;
    LIBSSH2_SESSION *m_session      = nullptr;
    LIBSSH2_SFTP    *m_sftp         = nullptr;
    QMutex          *m_sessionLock  = nullptr;
    QString          m_currentPath;
    QString          m_fsRoot;
    QString          m_distro;

    QLineEdit      *m_pathEdit      = nullptr;
    RemoteFileList *m_listWidget    = nullptr;
    QLabel         *m_statusLabel   = nullptr;
    QPushButton    *m_followBtn     = nullptr;

    // One row per queued file, in download order. A QListWidget with a painting
    // delegate rather than stacked child widgets: the list owns row geometry and
    // scrolling, so rows cannot be compressed onto each other.
    QListWidget              *m_progressList = nullptr;
    QList<QListWidgetItem*>   m_progressItems;
    QPushButton              *m_cancelBtn    = nullptr;

    // The download currently in flight, so it can be cancelled. Cleared when
    // the transfer ends by any route.
    SFTPWorker *m_activeDownload = nullptr;

    QList<SFTPWorker*> m_workers;

    // Download queue
    QList<QPair<QString,QString>> m_downloadQueue;
    int m_downloadTotal = 0;
    int m_downloadIndex = 0;
};
