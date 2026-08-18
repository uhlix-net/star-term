#include "remotebrowser.h"
#include "sessionpane.h"
#include "sshsession.h"
#include "wslsession.h"
#include "icons.h"

#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QMutex>
#include <QMutexLocker>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <cstring>
#include <functional>

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
            m_tabPending = false;
        } else if (ch == '\x7f' || ch == '\x08') {
            if (!m_buffer.isEmpty()) m_buffer.chop(1);
        } else if (ch == '\x03' || ch == '\x15') {
            m_buffer.clear();
            m_tabPending = false;
        } else if (ch == '\x1b') {
            m_escapeState = "esc";
        } else if (ch == '\t') {
            m_tabPending = true;
        } else if (ch.isPrint()) {
            m_tabPending = false;  // user typed a char; next server data is echo, not completion
            m_buffer += ch;
        }
    }
}

void CwdTracker::feedServerData(const QByteArray &data) {
    if (!m_tabPending) return;
    QString text = QString::fromUtf8(data);
    for (QChar ch : text) {
        if (!m_serverEscState.isEmpty()) {
            if (m_serverEscState == "esc") {
                m_serverEscState = (ch == '[') ? "csi" : "";
            } else if (m_serverEscState == "csi") {
                if (ch.isLetter() || ch == '~') m_serverEscState = "";
            }
            continue;
        }
        if (ch == '\x1b') { m_serverEscState = "esc"; continue; }
        if (ch == '\r' || ch == '\n') { m_tabPending = false; return; }
        if (ch.isPrint()) m_buffer += ch;
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
SFTPWorker::SFTPWorker(LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftp, QMutex *sessionLock,
                       Op op, QObject *parent)
    : QThread(parent), m_session(session), m_sftp(sftp), m_sessionLock(sessionLock), m_op(op)
{}

SFTPWorker::SFTPWorker(LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftp, QMutex *sessionLock,
                       const QString &localPath, const QString &remotePath,
                       QObject *parent)
    : QThread(parent), m_session(session), m_sftp(sftp), m_sessionLock(sessionLock)
    , m_op(Upload), m_localPath(localPath), m_remotePath(remotePath)
{}

SFTPWorker::SFTPWorker(LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftp, QMutex *sessionLock,
                       const QString &remotePath, const QString &localPath,
                       bool /*download*/, QObject *parent)
    : QThread(parent), m_session(session), m_sftp(sftp), m_sessionLock(sessionLock)
    , m_op(Download), m_localPath(localPath), m_remotePath(remotePath)
{}

SFTPWorker::SFTPWorker(LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftp, QMutex *sessionLock,
                       const QString &path, QObject *parent)
    : QThread(parent), m_session(session), m_sftp(sftp), m_sessionLock(sessionLock)
    , m_op(List), m_remotePath(path)
{}

// -----------------------------------------------------------------------
// Download progress rows
//
// Painted by a delegate rather than built from stacked child widgets, after
// three attempts using a QVBoxLayout of QLabel/QProgressBar pairs inside a
// QScrollArea all ended with the rows drawn on top of one another.
//
// Moving to a delegate was not enough on its own: rows still overlapped because
// setUniformItemSizes() cached the first row's measured height for every row,
// and that first measurement can happen before the view's font is applied — so
// every row was allotted less height than paint() draws into. Two things fix it
// for good: measure from the view's own font, and clip painting to the row rect
// so nothing can bleed into its neighbours whatever the view decides.
//
// Row layout is: filename on top, then groove, percentage and cancel glyph on
// one line beneath it.
// -----------------------------------------------------------------------
namespace {

constexpr int kBarHeight = 16;
constexpr int kRowPad    = 4;
constexpr int kGap       = 6;
constexpr int kCancelW   = 16;

// Geometry helpers, shared by paint() and the click handler so a hit test can
// never disagree with what was drawn.
int percentWidth(const QFontMetrics &fm) { return fm.horizontalAdvance("100%") + 4; }

QRect rowContent(const QRect &itemRect) {
    return itemRect.adjusted(kRowPad, kRowPad, -kRowPad, -kRowPad);
}

QRect cancelRectFor(const QRect &itemRect, const QFontMetrics &fm) {
    const QRect r = rowContent(itemRect);
    const int   y = r.top() + fm.height() + kRowPad;
    return QRect(r.right() - kCancelW, y + (kBarHeight - kCancelW) / 2,
                 kCancelW, kCancelW);
}

class DownloadRowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    // Called with the row index when its cancel glyph is clicked.
    std::function<void(int)> onCancelRow;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &) const override {
        // Take the font from the view rather than the style option: the option's
        // metrics can still be the default when the view first measures a row,
        // which yields a row shorter than paint() needs.
        const QFontMetrics fm(option.widget ? option.widget->font() : option.font);
        return QSize(0, fm.height() + kBarHeight + kRowPad * 3);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();

        painter->save();
        // Hard clip to this row. Whatever the view decided the row height is,
        // nothing here can bleed into the row above or below.
        painter->setClipRect(option.rect);

        opt.text.clear();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        const QFontMetrics fm     = option.fontMetrics;
        const QRect        r      = rowContent(option.rect);
        const QRect        textRect(r.left(), r.top(), r.width(), fm.height());
        const bool         active = index.data(DownloadActiveRole).toBool();

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(
            textRect, Qt::AlignLeft | Qt::AlignVCenter,
            fm.elidedText(index.data(Qt::DisplayRole).toString(),
                          Qt::ElideMiddle, textRect.width()));

        // Bottom line: groove, then the percentage, then the cancel glyph —
        // reading left to right, all on one line.
        const int pctW    = percentWidth(fm);
        const int barY    = textRect.bottom() + kRowPad;
        const int reserve = pctW + kGap + (active ? kCancelW + kGap : 0);
        const QRect barRect(r.left(), barY, qMax(24, r.width() - reserve), kBarHeight);

        // Draw the groove by hand rather than via QStyle::CE_ProgressBar. The app
        // installs a global stylesheet, so the style here is a QStyleSheetStyle;
        // asking it to draw a progress bar with a null widget gives it no styling
        // context to resolve against, and its CE_ProgressBar path then ignores the
        // rect it was handed — every row's groove landed in the same place while
        // everything painted directly (name, percentage, cancel) stayed correct.
        // Painting it here keeps the groove tied to this row's rect, whatever the
        // active style happens to be.
        const int pct = qBound(0, index.data(DownloadPercentRole).toInt(), 100);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(option.palette.color(QPalette::Mid), 1));
        painter->setBrush(option.palette.color(QPalette::Base));
        painter->drawRoundedRect(QRectF(barRect).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);

        if (pct > 0) {
            QRectF chunk(barRect);
            chunk.setWidth(barRect.width() * pct / 100.0);
            painter->setPen(Qt::NoPen);
            painter->setBrush(option.palette.color(QPalette::Highlight));
            painter->drawRoundedRect(chunk.adjusted(1, 1, -1, -1), 2, 2);
        }
        painter->restore();

        const QRect pctRect(barRect.right() + kGap, barY, pctW, kBarHeight);
        painter->drawText(pctRect, Qt::AlignRight | Qt::AlignVCenter,
                          index.data(DownloadStatusRole).toString());

        if (active) {
            const QRect c = cancelRectFor(option.rect, fm);
            painter->setPen(option.palette.color(QPalette::Text));
            painter->drawRect(c.adjusted(0, 0, -1, -1));
            const int inset = 4;
            const QRect x = c.adjusted(inset, inset, -inset - 1, -inset - 1);
            painter->drawLine(x.topLeft(), x.bottomRight());
            painter->drawLine(x.topRight(), x.bottomLeft());
        }

        painter->restore();
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override {
        if (event->type() != QEvent::MouseButtonRelease) return false;
        if (!index.data(DownloadActiveRole).toBool())     return false;
        auto *me = static_cast<QMouseEvent*>(event);
        if (!cancelRectFor(option.rect, option.fontMetrics).contains(me->pos())) return false;
        if (onCancelRow) onCancelRow(index.row());
        return true;
    }
};

} // namespace

// RAII helper: switch to blocking mode for the duration of a locked SFTP call,
// then restore non-blocking on exit. Always used while m_sessionLock is held.
struct BlockingGuard {
    LIBSSH2_SESSION *s;
    explicit BlockingGuard(LIBSSH2_SESSION *s) : s(s) { libssh2_session_set_blocking(s, 1); }
    ~BlockingGuard() { libssh2_session_set_blocking(s, 0); }
};

void SFTPWorker::run() {
    if (!m_session || !m_sftp || !m_sessionLock) {
        emit error("No SFTP session");
        return;
    }

    try {
        if (m_op == List) {
            LIBSSH2_SFTP_HANDLE *handle = nullptr;
            {
                QMutexLocker lock(m_sessionLock);
                BlockingGuard bg(m_session);
                QByteArray pathBytes = m_remotePath.toUtf8();
                handle = libssh2_sftp_opendir(m_sftp, pathBytes.constData());
            }
            if (!handle) {
                emit error(QString("Cannot open directory: %1").arg(m_remotePath));
                return;
            }

            QList<SFTPEntry> entries;
            while (true) {
                char buf[512];
                LIBSSH2_SFTP_ATTRIBUTES attrs{};
                int rc;
                {
                    QMutexLocker lock(m_sessionLock);
                    BlockingGuard bg(m_session);
                    rc = libssh2_sftp_readdir(handle, buf, sizeof(buf), &attrs);
                }
                if (rc <= 0) break;
                QString entryName = QString::fromUtf8(buf, rc);
                if (entryName == "." || entryName == "..") continue;
                bool isDir = (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
                    && LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
                qint64 sz = (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) ? (qint64)attrs.filesize : 0;
                entries.append({entryName, isDir, sz});
            }
            {
                QMutexLocker lock(m_sessionLock);
                BlockingGuard bg(m_session);
                libssh2_sftp_closedir(handle);
            }
            emit listed(m_remotePath, entries);

        } else if (m_op == Home) {
            char resolved[512] = {};
            int rc;
            {
                QMutexLocker lock(m_sessionLock);
                BlockingGuard bg(m_session);
                QByteArray dot = ".";
                rc = libssh2_sftp_realpath(m_sftp, dot.constData(),
                                           resolved, sizeof(resolved) - 1);
            }
            if (rc > 0) emit homeResolved(QString::fromUtf8(resolved, rc));
            else        emit error("Could not resolve home directory");

        } else if (m_op == Upload) {
            QFile localFile(m_localPath);
            if (!localFile.open(QIODevice::ReadOnly)) {
                emit error(QString("Cannot open local file: %1").arg(m_localPath));
                return;
            }

            LIBSSH2_SFTP_HANDLE *handle = nullptr;
            {
                QMutexLocker lock(m_sessionLock);
                BlockingGuard bg(m_session);
                QByteArray remoteBytes = m_remotePath.toUtf8();
                handle = libssh2_sftp_open(
                    m_sftp, remoteBytes.constData(),
                    LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
                    LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR |
                    LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);
            }
            if (!handle) {
                emit error(QString("Cannot create remote file: %1").arg(m_remotePath));
                return;
            }

            bool ok = true;
            QByteArray chunk;
            while (ok && !(chunk = localFile.read(32768)).isEmpty()) {
                const char *ptr = chunk.constData();
                size_t remaining = static_cast<size_t>(chunk.size());
                while (remaining > 0) {
                    ssize_t w;
                    {
                        QMutexLocker lock(m_sessionLock);
                        BlockingGuard bg(m_session);
                        w = libssh2_sftp_write(handle, ptr, remaining);
                    }
                    if (w < 0) { ok = false; break; }
                    ptr       += static_cast<size_t>(w);
                    remaining -= static_cast<size_t>(w);
                }
            }

            {
                QMutexLocker lock(m_sessionLock);
                BlockingGuard bg(m_session);
                libssh2_sftp_close(handle);
            }
            localFile.close();
            if (ok) emit transferred("upload", m_remotePath);
            else    emit error(QString("Write error uploading: %1").arg(m_remotePath));

        } else if (m_op == Download) {
            LIBSSH2_SFTP_HANDLE *handle = nullptr;
            qint64 total = 0;
            {
                QMutexLocker lock(m_sessionLock);
                BlockingGuard bg(m_session);
                QByteArray remoteBytes = m_remotePath.toUtf8();
                handle = libssh2_sftp_open(m_sftp, remoteBytes.constData(),
                                           LIBSSH2_FXF_READ, 0);
                if (handle) {
                    LIBSSH2_SFTP_ATTRIBUTES attrs{};
                    libssh2_sftp_fstat(handle, &attrs);
                    total = (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) ? (qint64)attrs.filesize : 0;
                }
            }
            if (!handle) {
                emit error(QString("Cannot open remote file: %1").arg(m_remotePath));
                return;
            }

            QFile localFile(m_localPath);
            if (!localFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QMutexLocker lock(m_sessionLock);
                BlockingGuard bg(m_session);
                libssh2_sftp_close(handle);
                emit error(QString("Cannot create local file: %1").arg(m_localPath));
                return;
            }

            qint64 done      = 0;
            bool   ok        = true;
            bool   wasCancelled = false;
            char   buf[32768];
            while (ok) {
                // Checked between chunks so a cancel takes effect within one
                // read rather than waiting for the whole file.
                if (m_cancelled.loadRelaxed()) { wasCancelled = true; break; }
                ssize_t nread;
                {
                    QMutexLocker lock(m_sessionLock);
                    BlockingGuard bg(m_session);
                    nread = libssh2_sftp_read(handle, buf, sizeof(buf));
                }
                if (nread == 0) break;
                if (nread < 0) { ok = false; break; }
                localFile.write(buf, nread);
                done += nread;
                if (total > 0) emit progress(done, total);
            }

            {
                QMutexLocker lock(m_sessionLock);
                BlockingGuard bg(m_session);
                libssh2_sftp_close(handle);
            }
            localFile.close();
            if (wasCancelled) {
                // A half-written file is worse than none — drop it.
                localFile.remove();
                emit cancelled(m_localPath);
            }
            else if (ok) emit transferred("download", m_localPath);
            else         emit error(QString("Read error downloading: %1").arg(m_remotePath));
        }
    } catch (...) {
        emit error("SFTP operation failed");
    }
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

// Download selected files to a temp dir, then hand off to Qt's drag machinery.
// The session mutex is held per-chunk so the SSH read loop can interleave.
void RemoteFileList::startDrag(Qt::DropActions /*supportedActions*/) {
    // WSL: the files are already reachable as Windows paths, so Explorer can
    // copy them straight from the distribution with nothing staged first.
    if (!fsRoot.isEmpty()) {
        const QString base = (remotePath == "/") ? fsRoot : fsRoot + remotePath;
        QList<QUrl> urls;
        for (QListWidgetItem *item : selectedItems()) {
            QVariantList data = item->data(Qt::UserRole).toList();
            if (data.size() >= 2 && !data[1].toBool())
                urls << QUrl::fromLocalFile(base + "/" + data[0].toString());
        }
        if (urls.isEmpty()) return;

        QMimeData *mime = new QMimeData;
        mime->setUrls(urls);
        QDrag *drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->exec(Qt::CopyAction);
        return;
    }

    if (!session || !sftp || !sessionLock) return;

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
    for (const QString &name : names) {
        QString remPath = remotePath + "/" + name;
        QString locPath = tmpDir.path() + "/" + name;

        LIBSSH2_SFTP_HANDLE *handle = nullptr;
        {
            QMutexLocker lock(sessionLock);
            BlockingGuard bg(session);
            QByteArray remBytes = remPath.toUtf8();
            handle = libssh2_sftp_open(sftp, remBytes.constData(), LIBSSH2_FXF_READ, 0);
        }
        if (!handle) continue;

        QFile f(locPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            char buf[32768];
            bool ok = true;
            while (ok) {
                ssize_t n;
                {
                    QMutexLocker lock(sessionLock);
                    BlockingGuard bg(session);
                    n = libssh2_sftp_read(handle, buf, sizeof(buf));
                }
                if (n == 0) break;
                if (n < 0) { ok = false; break; }
                f.write(buf, n);
            }
            f.close();
            if (ok) urls << QUrl::fromLocalFile(locPath);
        }

        {
            QMutexLocker lock(sessionLock);
            BlockingGuard bg(session);
            libssh2_sftp_close(handle);
        }
    }

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
    // Catch up straight away rather than waiting for the next cd — the session
    // has usually moved elsewhere while following was off.
    connect(m_followBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!on || !m_pane || !m_pane->cwdTracker) return;
        const QString cwd = m_pane->cwdTracker->cwd();
        if (!cwd.isEmpty() && cwd != m_currentPath) setPath(cwd);
        else                                        refresh();
    });

    // One progress row per queued file. Held in a scroll area with a capped
    // height so downloading many files scrolls instead of squeezing the list.
    m_progressList = new QListWidget;
    auto *rowDelegate = new DownloadRowDelegate(m_progressList);
    rowDelegate->onCancelRow = [this](int row) { cancelRow(row); };
    m_progressList->setItemDelegate(rowDelegate);
    m_progressList->setSelectionMode(QAbstractItemView::NoSelection);
    m_progressList->setFocusPolicy(Qt::NoFocus);
    m_progressList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_progressList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    // Deliberately NOT setUniformItemSizes: it caches the first row's measured
    // height for every row, and a hint measured before the view's font is
    // applied gives rows too short for what the delegate draws.
    m_progressList->setMaximumHeight(160);
    m_progressList->setVisible(false);

    // Whole-queue stop, alongside the per-row cancel in each item.
    m_cancelBtn = new QPushButton("Stop All Downloads");
    m_cancelBtn->setVisible(false);
    connect(m_cancelBtn, &QPushButton::clicked, this, &RemoteFileBrowser::cancelDownloads);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8,8,8,8);
    layout->setSpacing(8);
    layout->addWidget(title);
    layout->addWidget(pathRow);
    layout->addWidget(m_listWidget, 1);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_followBtn);
    layout->addWidget(m_progressList);
    layout->addWidget(m_cancelBtn);

    setEnabled(false);
}

void RemoteFileBrowser::setPane(SessionPane *pane) {
    if (m_pane == pane && connected()) return;

    if (m_pane && m_pane->cwdTracker) {
        disconnect(m_pane->cwdTracker, &CwdTracker::cwdChanged,
                   this, &RemoteFileBrowser::onCwdChanged);
    }

    m_pane        = pane;
    m_session     = nullptr;
    m_sftp        = nullptr;
    m_sessionLock = nullptr;
    m_fsRoot.clear();
    m_distro.clear();
    m_currentPath.clear();
    m_listWidget->clear();
    m_listWidget->session     = nullptr;
    m_listWidget->sftp        = nullptr;
    m_listWidget->sessionLock = nullptr;
    m_listWidget->fsRoot.clear();
    m_pathEdit->clear();
    m_statusLabel->setText("");

    if (!pane) { setEnabled(false); return; }

    if (pane->cwdTracker) {
        connect(pane->cwdTracker, &CwdTracker::cwdChanged,
                this, &RemoteFileBrowser::onCwdChanged);
    }

    // A WSL distribution has no SFTP server, but Windows exposes its filesystem
    // as a share, so the same panel drives ordinary file APIs instead.
    if (pane->connectionParams.value("type").toString() == "wsl") {
        m_distro = pane->connectionParams.value("distro").toString();
        m_fsRoot = wslFilesystemRoot(m_distro);
        if (m_fsRoot.isEmpty()) {
            setEnabled(false);
            m_statusLabel->setText("Distribution files unavailable");
            return;
        }
        m_listWidget->fsRoot = m_fsRoot;
        setEnabled(true);
        if (pane->cwdTracker && !pane->cwdTracker->cwd().isEmpty())
            setPath(pane->cwdTracker->cwd());
        else
            resolveHome();
        return;
    }

    if (SSHSession *ssh = qobject_cast<SSHSession*>(pane->session)) {
        // rawSftp() is safe here: it was set in the SSH thread before the
        // connected() signal was emitted, so the queued delivery establishes
        // a happens-before with this UI-thread call.
        m_session     = ssh->rawSession();
        m_sftp        = ssh->rawSftp();
        m_sessionLock = ssh->sessionLock();

        if (m_sftp) {
            m_listWidget->session     = m_session;
            m_listWidget->sftp        = m_sftp;
            m_listWidget->sessionLock = m_sessionLock;
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

QString RemoteFileBrowser::fsPathFor(const QString &posixPath) const {
    if (m_fsRoot.isEmpty()) return {};
    QString p = posixPath;
    if (!p.startsWith('/')) p.prepend('/');
    // The share root is itself the POSIX root, so "/" must not add a separator.
    return (p == "/") ? m_fsRoot : m_fsRoot + p;
}

void RemoteFileBrowser::listFilesystem(const QString &path) {
    QDir dir(fsPathFor(path));
    if (!dir.exists()) {
        m_listWidget->clear();
        m_statusLabel->setText("No access");
        return;
    }

    QList<SFTPEntry> entries;
    const QFileInfoList infos = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &fi : infos)
        entries.append({fi.fileName(), fi.isDir(), fi.size()});
    onListed(path, entries);
}

void RemoteFileBrowser::resolveHome() {
    if (fsMode()) {
        const QString home = wslHomeDirectory(m_distro);
        onHomeResolved(home.isEmpty() ? QStringLiteral("/") : home);
        return;
    }
    if (!m_sftp || !m_session || !m_pane || !m_pane->session) return;
    SFTPWorker *w = new SFTPWorker(m_session, m_sftp, m_sessionLock, SFTPWorker::Home, this);
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
    if (m_currentPath.isEmpty()) return;
    if (fsMode()) { listFilesystem(m_currentPath); return; }

    if (!m_sftp || !m_session) return;
    QString requestedPath = m_currentPath;
    SFTPWorker *w = new SFTPWorker(m_session, m_sftp, m_sessionLock, requestedPath, this);
    connect(w, &SFTPWorker::listed, this, &RemoteFileBrowser::onListed);
    connect(w, &SFTPWorker::error,  this, [this, requestedPath](const QString &) {
        if (requestedPath != m_currentPath) return;
        m_listWidget->clear();
        m_statusLabel->setText("No access");
    });
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

// The menu offers only what the click actually applies to: a file can be
// downloaded, empty space is the directory itself and can only receive an
// upload, and a directory offers neither.
void RemoteFileBrowser::onContextMenu(const QPoint &pos) {
    QListWidgetItem *item = m_listWidget->itemAt(pos);
    QMenu menu(this);

    if (!item) {
        QAction *uploadAction = menu.addAction("Upload...");
        if (menu.exec(m_listWidget->mapToGlobal(pos)) == uploadAction)
            onUploadDialog();
        return;
    }

    if (!item->isSelected()) {
        for (QListWidgetItem *sel : m_listWidget->selectedItems())
            sel->setSelected(false);
        item->setSelected(true);
        m_listWidget->setCurrentItem(item);
    }

    QStringList fileNames;
    for (QListWidgetItem *sel : m_listWidget->selectedItems()) {
        QVariantList data = sel->data(Qt::UserRole).toList();
        if (data.size() >= 2 && !data[1].toBool())
            fileNames << data[0].toString();
    }
    if (fileNames.isEmpty()) return;   // directories only — nothing to offer

    QAction *downloadAction = menu.addAction("Download...");
    if (menu.exec(m_listWidget->mapToGlobal(pos)) == downloadAction)
        onDownloadDialog(fileNames);
}

// Where the transfer dialogs should open. Without an explicit directory Qt starts
// in the process working directory, which for an installed build is the install
// folder — not somewhere anyone wants to save from or pick files out of.
static QString defaultTransferDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty() || !QDir(dir).exists())
        dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return dir;
}

void RemoteFileBrowser::onUploadDialog() {
    if (!connected() || m_currentPath.isEmpty()) return;
    QStringList paths = QFileDialog::getOpenFileNames(
        this, "Upload Files", defaultTransferDir());
    if (!paths.isEmpty()) onUploadRequested(paths);
}

void RemoteFileBrowser::onDownloadDialog(const QStringList &names) {
    if (!connected() || m_currentPath.isEmpty()) return;
    const QString startDir = defaultTransferDir();
    QList<QPair<QString,QString>> pairs;
    if (names.size() == 1) {
        QString localPath = QFileDialog::getSaveFileName(
            this, "Download File", QDir(startDir).filePath(names[0]));
        if (localPath.isEmpty()) return;
        pairs << qMakePair(names[0], localPath);
    } else {
        QString dir = QFileDialog::getExistingDirectory(this, "Download Files To", startDir);
        if (dir.isEmpty()) return;
        for (const QString &name : names)
            pairs << qMakePair(name, dir + "/" + name);
    }
    m_downloadQueue  = pairs;
    m_downloadTotal  = pairs.size();
    m_downloadIndex  = 0;
    buildProgressRows(pairs);
    startNextDownload();
}

// One row per queued file: name on the left, its own bar beneath. Rows are
// created up front so the whole queue is visible from the start rather than
// appearing one at a time.
void RemoteFileBrowser::buildProgressRows(const QList<QPair<QString,QString>> &pairs) {
    clearProgressRows();
    for (const auto &pair : pairs) {
        QListWidgetItem *item = new QListWidgetItem(pair.first, m_progressList);
        item->setToolTip(pair.second);                  // full destination path
        item->setData(DownloadPercentRole, 0);
        item->setData(DownloadStatusRole, QString("Queued"));
        item->setData(DownloadActiveRole, true);
        m_progressItems.append(item);
    }
    m_progressList->setVisible(!m_progressItems.isEmpty());
    m_cancelBtn->setEnabled(true);
    m_cancelBtn->setVisible(!m_progressItems.isEmpty());
}

// Stop the queue: drop anything not started, ask the running transfer to abort
// (it deletes its own partial file), and leave the rows up so it is clear where
// the download stopped.
void RemoteFileBrowser::cancelDownloads() {
    m_downloadQueue.clear();
    m_cancelBtn->setEnabled(false);
    if (m_activeDownload) {
        m_activeDownload->cancel();   // onDownloadCancelled() finishes the cleanup
        return;
    }
    // Nothing in flight (fsMode copies are synchronous) — tidy up here instead.
    const int idx = m_downloadIndex - 1;
    for (int i = idx + 1; i < m_progressItems.size(); ++i)
        setRowState(i, -1, "Cancelled", false);
    m_cancelBtn->setVisible(false);
    m_statusLabel->setText("Download stopped");
}

void RemoteFileBrowser::clearProgressRows() {
    m_progressItems.clear();
    if (m_progressList) {
        m_progressList->clear();       // owns and destroys its items
        m_progressList->setVisible(false);
    }
    if (m_cancelBtn) m_cancelBtn->setVisible(false);
}

// Set a row's bar and the text drawn beside it. Percent < 0 leaves it unchanged.
// active == false drops the cancel glyph, for rows that have finished one way or another.
void RemoteFileBrowser::setRowState(int index, int percent, const QString &status, bool active) {
    if (index < 0 || index >= m_progressItems.size()) return;
    QListWidgetItem *item = m_progressItems[index];
    if (percent >= 0) item->setData(DownloadPercentRole, percent);
    item->setData(DownloadStatusRole, status);
    item->setData(DownloadActiveRole, active);
}

bool RemoteFileBrowser::rowCancelled(int row) const {
    if (row < 0 || row >= m_progressItems.size()) return false;
    return m_progressItems[row]->data(DownloadStatusRole).toString() == "Cancelled";
}

// Cancel one file. The row index is the original queue position, which stays
// fixed; the queue itself is left intact and cancelled rows are skipped when
// their turn arrives, so row and queue positions cannot drift apart.
void RemoteFileBrowser::cancelRow(int row) {
    if (row < 0 || row >= m_progressItems.size()) return;
    const int active = m_downloadIndex - 1;
    if (row < active) return;                     // already finished

    if (row == active) {
        // In flight — the worker deletes its own partial file and reports back.
        setRowState(row, 0, "Cancelled", false);
        if (m_activeDownload) m_activeDownload->cancel();
        return;
    }
    setRowState(row, -1, "Cancelled", false);     // still queued; skipped later
}

void RemoteFileBrowser::onUploadRequested(const QStringList &localPaths) {
    if (m_currentPath.isEmpty()) return;

    if (fsMode()) {
        for (const QString &lp : localPaths) {
            const QString name = QFileInfo(lp).fileName();
            const QString dest = fsPathFor(m_currentPath) + "/" + name;
            // QFile::copy refuses to overwrite, so clear the way first.
            if (QFile::exists(dest)) QFile::remove(dest);
            if (!QFile::copy(lp, dest))
                onError(QString("Could not copy %1").arg(name));
        }
        refresh();
        return;
    }

    if (!m_sftp || !m_session) return;
    for (const QString &lp : localPaths) {
        QString remotePath = m_currentPath + "/" + QFileInfo(lp).fileName();
        SFTPWorker *w = new SFTPWorker(m_session, m_sftp, m_sessionLock, lp, remotePath, this);
        connect(w, &SFTPWorker::transferred, this, [this](const QString&, const QString&) { refresh(); });
        connect(w, &SFTPWorker::error, this, &RemoteFileBrowser::onError);
        runWorker(w);
    }
}

void RemoteFileBrowser::startNextDownload() {
    // Rows cancelled individually stay in the queue but are stepped over here,
    // so a row's index always matches its original position.
    QString name, localPath;
    for (;;) {
        if (m_downloadQueue.isEmpty()) {
            if (m_downloadIndex >= 1 && !rowCancelled(m_downloadIndex - 1))
                setRowState(m_downloadIndex - 1, 100, "Done", false);
            clearProgressRows();
            refresh();
            return;
        }
        auto pair = m_downloadQueue.takeFirst();
        ++m_downloadIndex;
        if (m_downloadIndex >= 2 && !rowCancelled(m_downloadIndex - 2))
            setRowState(m_downloadIndex - 2, 100, "Done", false);
        if (!rowCancelled(m_downloadIndex - 1)) {
            name      = pair.first;
            localPath = pair.second;
            break;
        }
    }
    QString remotePath = m_currentPath + "/" + name;
    const int curRow = m_downloadIndex - 1;
    if (curRow >= 0 && curRow < m_progressItems.size()) {
        setRowState(curRow, 0, "0%");
        m_progressList->scrollToItem(m_progressItems[curRow]);
    }
    if (m_downloadTotal > 1)
        m_statusLabel->setText(QString("Downloading %1 (%2/%3)...").arg(name).arg(m_downloadIndex).arg(m_downloadTotal));
    else
        m_statusLabel->setText(QString("Downloading %1...").arg(name));

    if (fsMode()) {
        const QString src = fsPathFor(m_currentPath) + "/" + name;
        // The save dialog already confirmed any overwrite.
        if (QFile::exists(localPath)) QFile::remove(localPath);
        if (QFile::copy(src, localPath)) onDownloadFinished(QString(), localPath);
        else onDownloadError(QString("Could not copy %1").arg(name));
        return;
    }

    SFTPWorker *w = new SFTPWorker(m_session, m_sftp, m_sessionLock, remotePath, localPath, true, this);
    connect(w, &SFTPWorker::progress,    this, &RemoteFileBrowser::onDownloadProgress);
    connect(w, &SFTPWorker::transferred, this, &RemoteFileBrowser::onDownloadFinished);
    connect(w, &SFTPWorker::error,       this, &RemoteFileBrowser::onDownloadError);
    connect(w, &SFTPWorker::cancelled,   this, &RemoteFileBrowser::onDownloadCancelled);
    m_activeDownload = w;
    runWorker(w);
}

void RemoteFileBrowser::onDownloadProgress(qint64 done, qint64 total) {
    if (total <= 0) return;
    const int idx = m_downloadIndex - 1;
    const int pct = static_cast<int>(done * 100 / total);
    setRowState(idx, pct, QString("%1%").arg(pct));
}

void RemoteFileBrowser::onDownloadFinished(const QString&, const QString&) {
    m_activeDownload = nullptr;
    startNextDownload();
}

void RemoteFileBrowser::onDownloadError(const QString &message) {
    m_activeDownload = nullptr;
    m_downloadQueue.clear();
    // Leave the rows up so it stays visible which file failed; mark the rest
    // abandoned rather than silently vanishing mid-queue.
    const int idx = m_downloadIndex - 1;
    for (int i = idx; i < m_progressItems.size(); ++i)
        setRowState(i, -1, i == idx ? "Failed" : "Cancelled", false);
    m_cancelBtn->setVisible(false);
    onError(message);
}

// The worker aborted mid-transfer and has already removed its partial file.
void RemoteFileBrowser::onDownloadCancelled(const QString&) {
    m_activeDownload = nullptr;
    m_downloadQueue.clear();
    const int idx = m_downloadIndex - 1;
    for (int i = idx; i < m_progressItems.size(); ++i)
        setRowState(i, i == idx ? 0 : -1, "Cancelled", false);
    m_cancelBtn->setVisible(false);
    m_statusLabel->setText("Download stopped");
    refresh();
}

void RemoteFileBrowser::runWorker(SFTPWorker *worker) {
    m_workers.append(worker);
    connect(worker, &SFTPWorker::finished, this, [this, worker]() {
        m_workers.removeAll(worker);
        // The transfer slots normally clear this first; belt-and-braces so the
        // pointer can never outlive the thread it refers to.
        if (m_activeDownload == worker) m_activeDownload = nullptr;
        worker->deleteLater();
    });
    worker->start();
}
