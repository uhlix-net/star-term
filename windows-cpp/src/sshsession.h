#pragma once
#include "terminalsession.h"

#include <QThread>
#include <QString>
#include <QByteArray>
#include <QMutex>
#include <QQueue>
#include <QPair>
#include <QWaitCondition>

#include <atomic>
#include <libssh2.h>
#include <libssh2_sftp.h>

// Replaces ssh_session.py (paramiko) with libssh2.
// Thread-safety model: all libssh2 calls are serialized by m_sessionMutex.
// The UI thread sends data/resize via lock-free queues; the SSH thread drains
// them in the read loop. SFTPWorker acquires m_sessionMutex per chunk.

class SSHSession : public TerminalSession {
    Q_OBJECT
public:
    SSHSession(
        const QString &host, int port,
        const QString &username,
        const QString &password    = QString(),
        const QString &keyPath     = QString(),
        const QString &keyPassphrase = QString(),
        const QString &term        = "vt100",
        int cols = 80, int rows = 24,
        QObject *parent = nullptr);

    ~SSHSession() override;

    // Called from UI thread — enqueue; SSH thread drains in read loop.
    void send(const QByteArray &data) override;
    void resize(int cols, int rows) override;

    // Interrupt the SSH thread and unblock any pending wait.
    void stop() override;

    // Returns the session lock; SFTPWorker must hold it around every libssh2 call.
    QMutex          *sessionLock()  { return &m_sessionMutex; }

    // Read-only accessors — safe after connected() signal has been delivered.
    LIBSSH2_SESSION *rawSession()   const { return m_session; }
    LIBSSH2_SFTP    *rawSftp()      const { return m_sftp; }

signals:
    // dataReceived / connectionError / connectionClosed / connected are
    // inherited from TerminalSession.

    // Emitted from SSH thread; UI thread must call acceptHostKey()/rejectHostKey().
    // mismatch == true means known_hosts already holds a DIFFERENT key for this
    // host (possible MITM), not merely that the host is new. storedFingerprint is
    // the previously trusted key in that case, and empty otherwise.
    void hostKeyUnknown(const QString &host, const QString &keyType,
                        const QString &fingerprint, const QString &storedFingerprint,
                        bool mismatch);

public slots:
    void acceptHostKey();
    void rejectHostKey();

protected:
    void run() override;

private:
    QString m_host;
    int     m_port;
    QString m_username;
    QString m_password;
    QString m_keyPath;
    QString m_keyPassphrase;
    QString m_term;
    int     m_cols, m_rows;

    std::atomic<bool>  m_running   { false };
    LIBSSH2_SESSION   *m_session   = nullptr;
    LIBSSH2_CHANNEL   *m_channel   = nullptr;
    LIBSSH2_SFTP      *m_sftp      = nullptr;
    int                m_sock      = -1;

    // Serializes ALL libssh2 session/channel/sftp calls across threads.
    QMutex                 m_sessionMutex;

    // UI → SSH thread command queues.
    QMutex                 m_queueMutex;
    QQueue<QByteArray>     m_sendQueue;
    QQueue<QPair<int,int>> m_resizeQueue;

    // Host-key prompt synchronization.
    QMutex         m_hostKeyMutex;
    QWaitCondition m_hostKeyCond;
    bool           m_hostKeyAnswered = false;
    bool           m_hostKeyAccepted = false;

    bool  connectSocket();
    bool  checkKnownHost(const char *fingerprint, size_t len,
                         int type, const char *key, size_t keyLen);
    void  drainQueues();
    void  closeSocket();
};
