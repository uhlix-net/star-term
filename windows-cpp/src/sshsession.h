#pragma once
#include <QThread>
#include <QString>
#include <QByteArray>
#include <QMutex>
#include <QWaitCondition>

#include <libssh2.h>
#include <libssh2_sftp.h>

// Replaces ssh_session.py (paramiko) with libssh2.

class SSHSession : public QThread {
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

    void send(const QByteArray &data);
    void resize(int cols, int rows);
    void stop();

    // Returns an open SFTP handle (or nullptr).
    // Called from the UI thread while the SSH thread is running.
    LIBSSH2_SFTP    *sftpHandle();

    // Returns the raw libssh2 session (for passing to SFTPWorker).
    LIBSSH2_SESSION *rawSession() const { return m_session; }

signals:
    void dataReceived(const QByteArray &data);
    void connectionError(const QString &msg);
    void connectionClosed();
    void connected();

    // Emitted from SSH thread; UI thread must respond and call acceptHostKey()/rejectHostKey()
    void hostKeyUnknown(const QString &host, const QString &keyType,
                        const QString &fingerprint, const QString &hexHash);

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

    bool           m_running       = false;
    LIBSSH2_SESSION *m_session     = nullptr;
    LIBSSH2_CHANNEL *m_channel     = nullptr;
    LIBSSH2_SFTP    *m_sftp        = nullptr;
    int             m_sock         = -1;

    // Host-key prompt synchronization
    QMutex         m_hostKeyMutex;
    QWaitCondition m_hostKeyCond;
    bool           m_hostKeyAnswered = false;
    bool           m_hostKeyAccepted = false;

    bool  connectSocket();
    bool  initSession();
    bool  authenticateAndShell();
    bool  checkKnownHost(const char *fingerprint, size_t len,
                         int type, const char *key, size_t keyLen);
    void  closeSocket();
};
