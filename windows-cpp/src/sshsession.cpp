#include "sshsession.h"
#include "config.h"
#include "debug.h"

#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QByteArray>
#include <QString>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   typedef SOCKET sock_t;
#  define INVALID_SOCK INVALID_SOCKET
#  define CLOSE_SOCK(s) closesocket(s)
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <fcntl.h>
   typedef int sock_t;
#  define INVALID_SOCK -1
#  define CLOSE_SOCK(s) ::close(s)
#endif

#include <openssl/sha.h>
#include <openssl/evp.h>

#include <cstring>

SSHSession::SSHSession(
    const QString &host, int port,
    const QString &username,
    const QString &password,
    const QString &keyPath,
    const QString &keyPassphrase,
    const QString &term,
    int cols, int rows,
    QObject *parent)
    : QThread(parent)
    , m_host(host), m_port(port)
    , m_username(username), m_password(password)
    , m_keyPath(keyPath), m_keyPassphrase(keyPassphrase)
    , m_term(term), m_cols(cols), m_rows(rows)
{}

SSHSession::~SSHSession() {
    stop();
    if (!wait(5000)) {
        terminate();
        wait(2000);
    }
}

// -----------------------------------------------------------------------
// Socket connect
// -----------------------------------------------------------------------
bool SSHSession::connectSocket() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    QByteArray hostBytes = m_host.toUtf8();
    QByteArray portBytes = QByteArray::number(m_port);
    int rc = getaddrinfo(hostBytes.constData(), portBytes.constData(), &hints, &res);
    if (rc != 0) return false;

    sock_t s = INVALID_SOCK;
    for (addrinfo *p = res; p; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == INVALID_SOCK) continue;
        if (::connect(s, p->ai_addr, static_cast<int>(p->ai_addrlen)) == 0) break;
        CLOSE_SOCK(s);
        s = INVALID_SOCK;
    }
    freeaddrinfo(res);
    if (s == INVALID_SOCK) return false;

    m_sock = static_cast<int>(s);
    return true;
}

void SSHSession::closeSocket() {
    if (m_sock >= 0) {
        CLOSE_SOCK(static_cast<sock_t>(m_sock));
        m_sock = -1;
    }
}

// -----------------------------------------------------------------------
// Known-hosts check
// -----------------------------------------------------------------------
static QString sha256FingerprintHex(const char *key, size_t keyLen) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(key), keyLen, digest);
    QByteArray b64 = QByteArray(reinterpret_cast<const char*>(digest), SHA256_DIGEST_LENGTH)
                         .toBase64(QByteArray::OmitTrailingEquals);
    return "SHA256:" + QString::fromLatin1(b64);
}

static QString libssh2TypeName(int type) {
    switch (type) {
    case LIBSSH2_HOSTKEY_TYPE_RSA:        return "ssh-rsa";
    case LIBSSH2_HOSTKEY_TYPE_DSS:        return "ssh-dss";
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_256:  return "ecdsa-sha2-nistp256";
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_384:  return "ecdsa-sha2-nistp384";
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_521:  return "ecdsa-sha2-nistp521";
    case LIBSSH2_HOSTKEY_TYPE_ED25519:    return "ssh-ed25519";
    default: return "unknown";
    }
}

bool SSHSession::checkKnownHost(const char *fingerprint, size_t /*len*/,
                                 int type, const char *key, size_t keyLen) {
    QString knownHostsPath = getKnownHostsPath();
    // Ensure parent directory exists (libssh2 writefile won't create it)
    QDir().mkpath(QFileInfo(knownHostsPath).absolutePath());

    LIBSSH2_KNOWNHOSTS *kh = libssh2_knownhost_init(m_session);
    if (!kh) return false;

    // Use native separators + local 8-bit encoding so libssh2's fopen() works
    // correctly on Windows (fopen uses ANSI codepage, not UTF-8)
    QByteArray pathBytes = QDir::toNativeSeparators(knownHostsPath).toLocal8Bit();
    libssh2_knownhost_readfile(kh, pathBytes.constData(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    struct libssh2_knownhost *found = nullptr;
    QByteArray hostBytes = m_host.toUtf8();
    int check = libssh2_knownhost_checkp(
        kh, hostBytes.constData(), m_port,
        key, keyLen,
        LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW,
        &found);

    if (check == LIBSSH2_KNOWNHOST_CHECK_MATCH) {
        libssh2_knownhost_free(kh);
        return true;
    }

    // Unknown or mismatched — prompt user.
    QString fingerprintStr = sha256FingerprintHex(key, keyLen);
    QString keyType        = libssh2TypeName(type);

    {
        QMutexLocker lock(&m_hostKeyMutex);
        m_hostKeyAnswered = false;
        m_hostKeyAccepted = false;
    }

    emit hostKeyUnknown(m_host, keyType, fingerprintStr,
                        QString::fromLatin1(QByteArray(key, static_cast<int>(qMin<size_t>(keyLen, 32))).toHex()));

    {
        QMutexLocker lock(&m_hostKeyMutex);
        while (!m_hostKeyAnswered)
            m_hostKeyCond.wait(&m_hostKeyMutex);
    }

    if (!m_hostKeyAccepted) {
        libssh2_knownhost_free(kh);
        return false;
    }

    QString hostWithPort = (m_port != 22)
        ? QString("[%1]:%2").arg(m_host).arg(m_port)
        : m_host;
    QByteArray hostWithPortBytes = hostWithPort.toUtf8();
    libssh2_knownhost_addc(
        kh, hostWithPortBytes.constData(), nullptr,
        key, keyLen,
        nullptr, 0,
        LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW,
        nullptr);
    libssh2_knownhost_writefile(kh, pathBytes.constData(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    libssh2_knownhost_free(kh);
    return true;
}

// -----------------------------------------------------------------------
// drainQueues — called from SSH thread only; flushes pending send/resize.
// Session must be in non-blocking mode on entry; restored on exit.
// -----------------------------------------------------------------------
void SSHSession::drainQueues() {
    QMutexLocker ql(&m_queueMutex);

    while (!m_sendQueue.isEmpty()) {
        QByteArray data = m_sendQueue.dequeue();
        ql.unlock();
        {
            QMutexLocker sl(&m_sessionMutex);
            libssh2_session_set_blocking(m_session, 1);
            const char *ptr = data.constData();
            size_t rem = static_cast<size_t>(data.size());
            while (rem > 0) {
                ssize_t w = libssh2_channel_write(m_channel, ptr, rem);
                if (w < 0) { rem = 0; break; }
                ptr += static_cast<size_t>(w);
                rem -= static_cast<size_t>(w);
            }
            libssh2_session_set_blocking(m_session, 0);
        }
        ql.relock();
    }

    if (!m_resizeQueue.isEmpty()) {
        auto [cols, rows] = m_resizeQueue.dequeue();
        // Discard any extra queued resizes; only the latest matters.
        while (!m_resizeQueue.isEmpty())
            m_resizeQueue.dequeue();
        ql.unlock();
        {
            QMutexLocker sl(&m_sessionMutex);
            libssh2_session_set_blocking(m_session, 1);
            libssh2_channel_request_pty_size(m_channel, cols, rows);
            libssh2_session_set_blocking(m_session, 0);
        }
        ql.relock();
    }
}

// -----------------------------------------------------------------------
// run() — SSH thread
// -----------------------------------------------------------------------
void SSHSession::run() {
    debugLog(QString("SSH connect: %1@%2:%3").arg(m_username, m_host).arg(m_port));

    auto emitError = [this](const QString &msg) {
        debugLog(QString("SSH error %1@%2:%3: %4").arg(m_username, m_host).arg(m_port).arg(msg));
        emit connectionError(msg);
    };

    // 1. TCP connect
    if (!connectSocket()) {
        emitError(QString("Could not connect to %1:%2").arg(m_host).arg(m_port));
        emit connectionClosed();
        return;
    }

    // 2. Init libssh2 session (blocking)
    m_session = libssh2_session_init();
    if (!m_session) {
        emitError("libssh2_session_init failed");
        closeSocket();
        emit connectionClosed();
        return;
    }
    libssh2_session_set_blocking(m_session, 1);

    int rc = libssh2_session_handshake(m_session, m_sock);
    if (rc) {
        char *errmsg = nullptr;
        libssh2_session_last_error(m_session, &errmsg, nullptr, 0);
        emitError(QString("SSH handshake failed: %1").arg(errmsg ? errmsg : ""));
        libssh2_session_free(m_session); m_session = nullptr;
        closeSocket();
        emit connectionClosed();
        return;
    }

    // 3. Check host key
    size_t keyLen = 0;
    int    keyType = 0;
    const char *key = libssh2_session_hostkey(m_session, &keyLen, &keyType);
    if (key) {
        const char *fingerprint = libssh2_hostkey_hash(m_session, LIBSSH2_HOSTKEY_HASH_SHA256);
        if (!checkKnownHost(fingerprint ? fingerprint : "", 32, keyType, key, keyLen)) {
            emitError(QString("Host key for %1 was not accepted").arg(m_host));
            libssh2_session_disconnect(m_session, "Host key rejected");
            libssh2_session_free(m_session); m_session = nullptr;
            closeSocket();
            emit connectionClosed();
            return;
        }
    }

    // 4. Authentication
    QByteArray usernameBytes = m_username.toUtf8();
    if (!m_keyPath.isEmpty()) {
        QByteArray keyPathBytes       = m_keyPath.toUtf8();
        QByteArray keyPassphraseBytes = m_keyPassphrase.toUtf8();
        rc = libssh2_userauth_publickey_fromfile(
            m_session,
            usernameBytes.constData(),
            nullptr,
            keyPathBytes.constData(),
            m_keyPassphrase.isEmpty() ? nullptr : keyPassphraseBytes.constData()
        );
    } else {
        QByteArray passwordBytes = m_password.toUtf8();
        rc = libssh2_userauth_password(
            m_session,
            usernameBytes.constData(),
            passwordBytes.constData()
        );
    }

    if (rc) {
        char *errmsg = nullptr;
        libssh2_session_last_error(m_session, &errmsg, nullptr, 0);
        emitError(QString("Authentication failed: %1").arg(errmsg ? errmsg : ""));
        libssh2_session_disconnect(m_session, "Auth failed");
        libssh2_session_free(m_session); m_session = nullptr;
        closeSocket();
        emit connectionClosed();
        return;
    }

    // 5. Open channel + PTY + shell
    m_channel = libssh2_channel_open_session(m_session);
    if (!m_channel) {
        emitError("Could not open SSH channel");
        libssh2_session_disconnect(m_session, "");
        libssh2_session_free(m_session); m_session = nullptr;
        closeSocket();
        emit connectionClosed();
        return;
    }

    QByteArray termBytes = m_term.toUtf8();
    rc = libssh2_channel_request_pty_ex(
        m_channel,
        termBytes.constData(), static_cast<unsigned>(termBytes.size()),
        nullptr, 0,
        m_cols, m_rows, 0, 0);
    if (rc) {
        emitError("PTY request failed");
        libssh2_channel_free(m_channel); m_channel = nullptr;
        libssh2_session_disconnect(m_session, "");
        libssh2_session_free(m_session); m_session = nullptr;
        closeSocket();
        emit connectionClosed();
        return;
    }

    rc = libssh2_channel_shell(m_channel);
    if (rc) {
        emitError("Shell request failed");
        libssh2_channel_free(m_channel); m_channel = nullptr;
        libssh2_session_disconnect(m_session, "");
        libssh2_session_free(m_session); m_session = nullptr;
        closeSocket();
        emit connectionClosed();
        return;
    }

    // 6. Initialize SFTP subsystem while still in blocking mode.
    // m_sftp may be nullptr if the server doesn't support SFTP; that's OK.
    m_sftp = libssh2_sftp_init(m_session);

    // Switch to non-blocking for the read loop.
    libssh2_session_set_blocking(m_session, 0);

    debugLog(QString("SSH connected: %1@%2:%3").arg(m_username, m_host).arg(m_port));
    emit connected();

    // 7. Read loop — SSH thread only.
    // Invariant: m_sessionMutex is not held between iterations;
    // session is in non-blocking mode whenever the mutex is released.
    m_running = true;
    QByteArray buf(4096, '\0');

    while (m_running) {
        drainQueues();

        ssize_t nread;
        bool    eof = false;
        {
            QMutexLocker sl(&m_sessionMutex);
            nread = libssh2_channel_read(
                m_channel, buf.data(), static_cast<size_t>(buf.size()));
            if (nread == 0)
                eof = libssh2_channel_eof(m_channel) != 0;
        }

        if (nread > 0) {
            emit dataReceived(buf.left(static_cast<int>(nread)));
        } else if (nread == LIBSSH2_ERROR_EAGAIN) {
            msleep(10);
        } else {
            // nread == 0 + EOF, or error
            break;
        }
    }

    // 8. Clean up — all libssh2 calls under the session mutex.
    {
        QMutexLocker sl(&m_sessionMutex);
        libssh2_session_set_blocking(m_session, 1);
        if (m_sftp) {
            libssh2_sftp_shutdown(m_sftp);
            m_sftp = nullptr;
        }
        if (m_channel) {
            libssh2_channel_close(m_channel);
            libssh2_channel_free(m_channel);
            m_channel = nullptr;
        }
        libssh2_session_disconnect(m_session, "Bye");
        libssh2_session_free(m_session);
        m_session = nullptr;
    }
    closeSocket();

    debugLog(QString("SSH connection closed: %1@%2:%3").arg(m_username, m_host).arg(m_port));
    emit connectionClosed();
}

// -----------------------------------------------------------------------
// Public interface — UI thread
// -----------------------------------------------------------------------

// Enqueue data; SSH thread writes it in drainQueues().
void SSHSession::send(const QByteArray &data) {
    if (data.isEmpty()) return;
    QMutexLocker lock(&m_queueMutex);
    m_sendQueue.enqueue(data);
}

// Enqueue resize; SSH thread applies it in drainQueues().
void SSHSession::resize(int cols, int rows) {
    m_cols = cols;
    m_rows = rows;
    QMutexLocker lock(&m_queueMutex);
    m_resizeQueue.enqueue({cols, rows});
}

// Close the socket to interrupt any blocking libssh2 call, then let the
// read loop exit naturally via m_running == false.
void SSHSession::stop() {
    m_running = false;
    closeSocket();
    QMutexLocker lock(&m_hostKeyMutex);
    m_hostKeyAnswered = true;
    m_hostKeyAccepted = false;
    m_hostKeyCond.wakeAll();
}

void SSHSession::acceptHostKey() {
    QMutexLocker lock(&m_hostKeyMutex);
    m_hostKeyAccepted = true;
    m_hostKeyAnswered = true;
    m_hostKeyCond.wakeAll();
}

void SSHSession::rejectHostKey() {
    QMutexLocker lock(&m_hostKeyMutex);
    m_hostKeyAccepted = false;
    m_hostKeyAnswered = true;
    m_hostKeyCond.wakeAll();
}
