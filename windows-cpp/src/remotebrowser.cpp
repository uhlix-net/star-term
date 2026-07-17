#include "remotebrowser.h"
#include "sessionpane.h"
#include "sshsession.h"
#include "icons.h"

#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QTemporaryDir>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <cstring>

// -----------------------------------------------------------------------
// CwdTracker
// -----------------------------------------------------------------------
CwdTracker::CwdTracker(QObject *parent) : QObject(parent) {}

void CwdTracker::setHome(const QString &home) {
    m_home = home;
    if (m_cwd.isEmpty()) {
        m_cwd = home;
        emit cwdChanged(m_cwd);
    }
}

void CwdTracker::feedInput(const QByteArray &data) {
    QString text = QString::fromUtf8(data);
    for (QChar ch : text) {
        if (!m_escapeState.isEmpty()) {
            consumeEscape(ch);
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            processLine(m_buffer);
            m_buffer.clear();
        } else if (ch == '\x7f' || ch == '\x08') {
            if (!m_buffer.isEmpty()) m_buffer.chop(1);
        } else if (ch == '\x03' || ch == '\x15') {
            m_buffer.clear();
        } else if (ch == '\x1b') {
            m_escapeState = "esc";
        } else if (ch.isPrint()) {
            m_buffer += ch;
        }
    }
}

void CwdTracker::consumeEscape(QChar ch) {
    if (m_escapeState == "esc") {
        m_escapeState = (ch == '[') ? "csi" : "";
    } else if (m_escapeState == "csi") {
        if (ch.isLetter() || ch == '~') m_escapeState = "";
    }
}

void CwdTracker::processLine(const QString &line) {
    QString l = line.trimmed();
    if (l == "cd" || l.startsWith("cd ") || l.startsWith("cd\t")) {
        QString arg = l.mid(2).trimmed();
        QString newCwd = resolve(arg);
        if (!newCwd.isEmpty() && newCwd != m_cwd) {
            m_cwd = newCwd;
            emit cwdChanged(m_cwd);
        }
    }
}

static QString posixNormpath(const QString &path) {
    QStringList parts = path.split('/');
    QStringList out;
    for (const QString &p : parts) {
        if (p == "" || p == ".") continue;
        if (p == "..") { if (!out.isEmpty()) out.removeLast(); }
        else out.append(p);
    }
    return "/" + out.join('/');
}

QString CwdTracker::resolve(const QString &arg) {
    if (arg.isEmpty()) return m_home;
    if (arg == "-") return QString();
    if (arg == "~") return m_home;
    if (arg.startsWith("~/")) {
        if (m_home.isEmpty()) return QString();
        return posixNormpath(m_home + "/" + arg.mid(2));
    }
    if (arg.startsWith("/")) return posixNormpath(arg);
    if (!m_cwd.isEmpty()) return posixNormpath(m_cwd + "/" + arg);
    return QString();
}

// -----------------------------------------------------------------------
// SFTPWorker
// -----------------------------------------------------------------------
SFTPWorker::SFTPWorker(LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftp,
                       Op op, QObject *parent)
    : QThread(parent), m_session(session), m_sftp(sftp), m_op(op)
{}

SFTPWorker::SFTPWorker(LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftp,
                       const QString &localPath, const QString &remotePath,
                       QObject *parent)
    : QThread(parent), m_session(session), m_sftp(sftp), m_op(Upload)
    , m_localPath(localPath), m_remotePath(remotePath)
{}

SFTPWorker::SFTPWorker(LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftp,
                       const QString &remotePath, const QString &localPath,
                       bool /*download*/, QObject *parent)
    : QThread(parent), m_session(session), m_sftp(sftp), m_op(Download)
    , m_localPath(localPath), m_remotePath(remotePath)
{}

SFTPWorker::SFTPWorker(LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftp,
                       const QString &path, QObject *parent)
    : QThread(parent), m_session(session), m_sftp(sftp), m_op(List)
    , m_remotePath(path)
{}

void SFTPWorker::run() {
    if (!m_session || !m_sftp) {
        emit error("No SFTP session");
        return;
    }

    libssh2_session_set_blocking(m_session, 1);

    try {
        if (m_op == List) {
            QByteArray pathBytes = m_remotePath.toUtf8();
            LIBSSH2_SFTP_HANDLE *handle =
                libssh2_sftp_opendir(m_sftp, pathBytes.constData());
            if (!handle) {
                emit error(QString("Cannot open directory: %1").arg(m_remotePath));
                libssh2_session_set_blocking(m_session, 0);
                return;
            }

            QList<SFTPEntry> entries;
            char buf[512];
            LIBSSH2_SFTP_ATTRIBUTES attrs;
            while (true) {
                int rc = libssh2_sftp_readdir(handle, buf, sizeof(buf), &attrs);
                if (rc <= 0) break;
                QString entryName = QString::fromUtf8(buf, rc);
                if (entryName == "." || entryName == "..") continue;
                bool isDir = (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
                    && LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
                qint64 sz = (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) ? (qint64)attrs.filesize : 0;
                entries.append({entryName, isDir, sz});
            }
            libssh2_sftp_closedir(handle);
            emit listed(m_remotePath, entries);

        } else if (m_op == Home) {
            char resolved[512] = {};
            QByteArray dot = ".";
            int rc = libssh2_sftp_realpath(m_sftp, dot.constData(),
                                           resolved, sizeof(resolved) - 1);
            if (rc > 0) emit homeResolved(QString::fromUtf8(resolved, rc));
            else        emit error("Could not resolve home directory");

        } else if (m_op == Upload) {
            QFile localFile(m_localPath);
            if (!localFile.open(QIODevice::ReadOnly)) {
                emit error(QString("Cannot open local file: %1").arg(m_localPath));
                libssh2_session_set_blocking(m_session, 0);
                return;
            }

            QByteArray remoteBytes = m_remotePath.toUtf8();
            LIBSSH2_SFTP_HANDLE *handle =
                libssh2_sftp_open(m_sftp, remoteBytes.constData(),
                                  LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
                                  LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR |
                                  LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);
            if (!handle) {
                emit error(QString("Cannot create remote file: %1").arg(m_remotePath));
                libssh2_session_set_blocking(m_session, 0);
                return;
            }

            QByteArray chunk;
            while (!(chunk = localFile.read(32768)).isEmpty()) {
                const char *ptr = chunk.constData();
                size_t remaining = static_cast<size_t>(chunk.size());
                while (remaining > 0) {
                    ssize_t w = libssh2_sftp_write(handle, ptr, remaining);
                    if (w < 0) break;
                    ptr       += w;
                    remaining -= static_cast<size_t>(w);
                }
            }
            libssh2_sftp_close(handle);
            localFile.close();
            emit transferred("upload", m_remotePath);

        } else if (m_op == Download) {
            QByteArray remoteBytes = m_remotePath.toUtf8();
            LIBSSH2_SFTP_HANDLE *handle =
                libssh2_sftp_open(m_sftp, remoteBytes.constData(),
                                  LIBSSH2_FXF_READ, 0);
            if (!handle) {
                emit error(QString("Cannot open remote file: %1").arg(m_remotePath));
                libssh2_session_set_blocking(m_session, 0);
                return;
            }

            // Get size via stat
            LIBSSH2_SFTP_ATTRIBUTES attrs{};
            libssh2_sftp_fstat(handle, &attrs);
            qint64 total = (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) ? (qint64)attrs.filesize : 0;

            QFile localFile(m_localPath);
            if (!localFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                emit error(QString("Cannot create local file: %1").arg(m_localPath));
                libssh2_sftp_close(handle);
                libssh2_session_set_blocking(m_session, 0);
                return;
            }

            char buf[32768];
            qint64 done = 0;
            ssize_t nread;
            while ((nread = libssh2_sftp_read(handle, buf, sizeof(buf))) > 0) {
                localFile.write(buf, nread);
                done += nread;
                if (total > 0) emit progress(done, total);
            }
            libssh2_sftp_close(handle);
            localFile.close();
            emit transferred("download", m_localPath);
        }
    } catch (...) {
        emit error("SFTP operation failed");
    }

    libssh2_session_set_blocking(m_session, 0);
}

// -----------------------------------------------------------------------
// RemoteFileList
// -----------------------------------------------------------------------
RemoteFileList::RemoteFileList(QWidget *parent) : QListWidget(parent) {
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DragDrop);
}

void RemoteFileList::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else QListWidget::dragEnterEvent(event);
}

void RemoteFileList::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else QListWidget::dragMoveEvent(event);
}

void RemoteFileList::dropEvent(QDropEvent *event) {
    if (event->mimeData()->hasUrls()) {
        QStringList paths;
        for (const QUrl &url : event->mimeData()->urls())
            if (url.isLocalFile()) paths << url.toLocalFile();
        if (!paths.isEmpty()) emit uploadRequested(paths);
        event->acceptProposedAction();
    } else {
        QListWidget::dropEvent(event);
    }
}

void RemoteFileList::startDrag(Qt::DropActions /*supportedActions*/) {
    if (!session || !sftp) return;

    QStringList names;
    for (QListWidgetItem *item : selectedItems()) {
        QVariantList data = item->data(Qt::UserRole).toList();
        if (data.size() >= 2 && !data[1].toBool())
            names << data[0].toString();
    }
    if (names.isEmpty()) return;

    QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) return;

    QList<QUrl> urls;
    libssh2_session_set_blocking(session, 1);
    for (const QString &name : names) {
        QString remPath = remotePath + "/" + name;
        QString locPath = tmpDir.path() + "/" + name;

        QByteArray remBytes = remPath.toUtf8();
        LIBSSH2_SFTP_HANDLE *handle =
            libssh2_sftp_open(sftp, remBytes.constData(), LIBSSH2_FXF_READ, 0);
        if (!handle) continue;

        QFile f(locPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            char buf[32768];
            ssize_t n;
            while ((n = libssh2_sftp_read(handle, buf, sizeof(buf))) > 0)
                f.write(buf, n);
            f.close();
            urls << QUrl::fromLocalFile(locPath);
        }
        libssh2_sftp_close(handle);
    }
    libssh2_session_set_blocking(session, 0);

    if (urls.isEmpty()) return;

    QMimeData *mime = new QMimeData;
    mime->setUrls(urls);
    QDrag *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::CopyAction);
}

// -----------------------------------------------------------------------
// RemoteFileBrowser
// -----------------------------------------------------------------------
RemoteFileBrowser::RemoteFileBrowser(QWidget *parent) : QWidget(parent) {
    QLabel *title = new QLabel("Remote Files");
    title->setObjectName("sectionTitle");

    m_pathEdit = new QLineEdit;
    connect(m_pathEdit, &QLineEdit::returnPressed, this, &RemoteFileBrowser::onPathEntered);

    QToolButton *upBtn = new QToolButton;
    upBtn->setIcon(Icons::upIcon());
    upBtn->setToolTip("Up one directory");
    connect(upBtn, &QToolButton::clicked, this, &RemoteFileBrowser::goUp);

    QToolButton *refreshBtn = new QToolButton;
    refreshBtn->setIcon(Icons::refreshIcon());
    refreshBtn->setToolTip("Refresh");
    connect(refreshBtn, &QToolButton::clicked, this, &RemoteFileBrowser::refresh);

    QWidget *pathRow = new QWidget;
    QHBoxLayout *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0,0,0,0);
    pathLayout->setSpacing(4);
    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(upBtn);
    pathLayout->addWidget(refreshBtn);

    m_listWidget = new RemoteFileList;
    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, &RemoteFileBrowser::onItemDoubleClicked);
    connect(m_listWidget, &RemoteFileList::uploadRequested,
            this, &RemoteFileBrowser::onUploadRequested);
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listWidget, &QListWidget::customContextMenuRequested,
            this, &RemoteFileBrowser::onContextMenu);

    m_statusLabel = new QLabel;
    m_statusLabel->setStyleSheet("color: #8a8a8a;");

    m_followBtn = new QPushButton("Follow Current Directory");
    m_followBtn->setCheckable(true);
    m_followBtn->setChecked(true);

    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setVisible(false);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8,8,8,8);
    layout->setSpacing(8);
    layout->addWidget(title);
    layout->addWidget(pathRow);
    layout->addWidget(m_listWidget, 1);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_followBtn);
    layout->addWidget(m_progressBar);

    setEnabled(false);
}

void RemoteFileBrowser::setPane(SessionPane *pane) {
    if (m_pane == pane && m_sftp != nullptr) return;

    // Disconnect old pane signals
    if (m_pane && m_pane->cwdTracker) {
        disconnect(m_pane->cwdTracker, &CwdTracker::cwdChanged,
                   this, &RemoteFileBrowser::onCwdChanged);
    }

    m_pane        = pane;
    m_session     = nullptr;
    m_sftp        = nullptr;
    m_currentPath.clear();
    m_listWidget->clear();
    m_listWidget->session  = nullptr;
    m_listWidget->sftp     = nullptr;
    m_pathEdit->clear();
    m_statusLabel->setText("");

    if (!pane) { setEnabled(false); return; }

    if (pane->cwdTracker) {
        connect(pane->cwdTracker, &CwdTracker::cwdChanged,
                this, &RemoteFileBrowser::onCwdChanged);
    }

    if (pane->session) {
        m_session = pane->session->rawSession();
        m_sftp    = pane->session->sftpHandle();
        if (m_sftp) {
            m_listWidget->session = m_session;
            m_listWidget->sftp    = m_sftp;
            setEnabled(true);
            if (pane->cwdTracker && !pane->cwdTracker->cwd().isEmpty())
                setPath(pane->cwdTracker->cwd());
            else
                resolveHome();
        } else {
            setEnabled(false);
        }
    } else {
        setEnabled(false);
    }
}

void RemoteFileBrowser::resolveHome() {
    if (!m_sftp || !m_session || !m_pane || !m_pane->session) return;
    SFTPWorker *w = new SFTPWorker(m_session, m_sftp, SFTPWorker::Home, this);
    connect(w, &SFTPWorker::homeResolved, this, &RemoteFileBrowser::onHomeResolved);
    connect(w, &SFTPWorker::error, this, &RemoteFileBrowser::onError);
    runWorker(w);
}

void RemoteFileBrowser::onHomeResolved(const QString &home) {
    if (m_pane && m_pane->cwdTracker)
        m_pane->cwdTracker->setHome(home);
    setPath(home);
}

void RemoteFileBrowser::setPath(const QString &path) {
    m_currentPath = path;
    m_pathEdit->setText(path);
    refresh();
}

void RemoteFileBrowser::refresh() {
    if (!m_sftp || !m_session || m_currentPath.isEmpty()) return;
    SFTPWorker *w = new SFTPWorker(m_session, m_sftp, m_currentPath, this);
    connect(w, &SFTPWorker::listed,  this, &RemoteFileBrowser::onListed);
    connect(w, &SFTPWorker::error,   this, &RemoteFileBrowser::onError);
    runWorker(w);
}

void RemoteFileBrowser::onListed(const QString &path, const QList<SFTPEntry> &entries) {
    if (path != m_currentPath) return;
    m_listWidget->clear();
    m_listWidget->remotePath = path;

    QList<SFTPEntry> sorted = entries;
    std::sort(sorted.begin(), sorted.end(), [](const SFTPEntry &a, const SFTPEntry &b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.name.toLower() < b.name.toLower();
    });

    for (const SFTPEntry &e : sorted) {
        QListWidgetItem *item = new QListWidgetItem(e.name);
        item->setIcon(e.isDir ? Icons::directoryIcon() : Icons::fileIcon());
        QVariantList data = {e.name, e.isDir};
        item->setData(Qt::UserRole, data);
        m_listWidget->addItem(item);
    }
    m_statusLabel->setText(QString("%1 item(s)").arg(sorted.size()));
}

void RemoteFileBrowser::onError(const QString &message) {
    m_statusLabel->setText("Error: " + message);
}

void RemoteFileBrowser::onItemDoubleClicked(QListWidgetItem *item) {
    QVariantList data = item->data(Qt::UserRole).toList();
    if (data.size() >= 2 && data[1].toBool() && !m_currentPath.isEmpty()) {
        QString newPath = m_currentPath + "/" + data[0].toString();
        // normalize
        QStringList parts = newPath.split('/', Qt::SkipEmptyParts);
        QStringList out;
        for (const QString &p : parts) {
            if (p == ".") continue;
            if (p == "..") { if (!out.isEmpty()) out.removeLast(); }
            else out << p;
        }
        setPath("/" + out.join('/'));
    }
}

void RemoteFileBrowser::onPathEntered() {
    QString path = m_pathEdit->text().trimmed();
    if (!path.isEmpty()) setPath(path);
}

void RemoteFileBrowser::goUp() {
    if (m_currentPath.isEmpty() || m_currentPath == "/") return;
    int idx = m_currentPath.lastIndexOf('/');
    setPath(idx > 0 ? m_currentPath.left(idx) : "/");
}

void RemoteFileBrowser::onCwdChanged(const QString &path) {
    if (m_followBtn->isChecked()) setPath(path);
}

void RemoteFileBrowser::onContextMenu(const QPoint &pos) {
    QListWidgetItem *item = m_listWidget->itemAt(pos);
    QStringList fileNames;
    if (item) {
        if (!item->isSelected()) {
            for (QListWidgetItem *sel : m_listWidget->selectedItems())
                sel->setSelected(false);
            item->setSelected(true);
            m_listWidget->setCurrentItem(item);
        }
        for (QListWidgetItem *sel : m_listWidget->selectedItems()) {
            QVariantList data = sel->data(Qt::UserRole).toList();
            if (data.size() >= 2 && !data[1].toBool())
                fileNames << data[0].toString();
        }
    }

    QMenu menu(this);
    QAction *downloadAction = nullptr;
    if (!fileNames.isEmpty())
        downloadAction = menu.addAction("Download...");
    QAction *uploadAction = menu.addAction("Upload...");
    QAction *chosen = menu.exec(m_listWidget->mapToGlobal(pos));
    if (downloadAction && chosen == downloadAction)
        onDownloadDialog(fileNames);
    else if (chosen == uploadAction)
        onUploadDialog();
}

void RemoteFileBrowser::onUploadDialog() {
    if (!m_sftp || m_currentPath.isEmpty()) return;
    QStringList paths = QFileDialog::getOpenFileNames(this, "Upload Files");
    if (!paths.isEmpty()) onUploadRequested(paths);
}

void RemoteFileBrowser::onDownloadDialog(const QStringList &names) {
    if (!m_sftp || m_currentPath.isEmpty()) return;
    QList<QPair<QString,QString>> pairs;
    if (names.size() == 1) {
        QString localPath = QFileDialog::getSaveFileName(this, "Download File", names[0]);
        if (localPath.isEmpty()) return;
        pairs << qMakePair(names[0], localPath);
    } else {
        QString dir = QFileDialog::getExistingDirectory(this, "Download Files To");
        if (dir.isEmpty()) return;
        for (const QString &name : names)
            pairs << qMakePair(name, dir + "/" + name);
    }
    m_downloadQueue  = pairs;
    m_downloadTotal  = pairs.size();
    m_downloadIndex  = 0;
    startNextDownload();
}

void RemoteFileBrowser::onUploadRequested(const QStringList &localPaths) {
    if (!m_sftp || !m_session || m_currentPath.isEmpty()) return;
    for (const QString &lp : localPaths) {
        QString remotePath = m_currentPath + "/" + QFileInfo(lp).fileName();
        SFTPWorker *w = new SFTPWorker(m_session, m_sftp, lp, remotePath, this);
        connect(w, &SFTPWorker::transferred, this, [this](const QString&, const QString&) { refresh(); });
        connect(w, &SFTPWorker::error, this, &RemoteFileBrowser::onError);
        runWorker(w);
    }
}

void RemoteFileBrowser::startNextDownload() {
    if (m_downloadQueue.isEmpty()) {
        m_progressBar->setVisible(false);
        refresh();
        return;
    }
    auto [name, localPath] = m_downloadQueue.takeFirst();
    ++m_downloadIndex;
    QString remotePath = m_currentPath + "/" + name;
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    if (m_downloadTotal > 1)
        m_statusLabel->setText(QString("Downloading %1 (%2/%3)...").arg(name).arg(m_downloadIndex).arg(m_downloadTotal));
    else
        m_statusLabel->setText(QString("Downloading %1...").arg(name));

    SFTPWorker *w = new SFTPWorker(m_session, m_sftp, remotePath, localPath, true, this);
    connect(w, &SFTPWorker::progress,     this, &RemoteFileBrowser::onDownloadProgress);
    connect(w, &SFTPWorker::transferred,  this, &RemoteFileBrowser::onDownloadFinished);
    connect(w, &SFTPWorker::error,        this, &RemoteFileBrowser::onDownloadError);
    runWorker(w);
}

void RemoteFileBrowser::onDownloadProgress(qint64 done, qint64 total) {
    if (total > 0) m_progressBar->setValue(static_cast<int>(done * 100 / total));
}

void RemoteFileBrowser::onDownloadFinished(const QString&, const QString&) {
    startNextDownload();
}

void RemoteFileBrowser::onDownloadError(const QString &message) {
    m_downloadQueue.clear();
    m_progressBar->setVisible(false);
    onError(message);
}

void RemoteFileBrowser::runWorker(SFTPWorker *worker) {
    m_workers.append(worker);
    connect(worker, &SFTPWorker::finished, this, [this, worker]() {
        m_workers.removeAll(worker);
        worker->deleteLater();
    });
    worker->start();
}
