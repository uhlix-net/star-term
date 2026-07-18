#pragma once
#include <QThread>
#include <QJsonObject>
#include <QMutex>

#include <atomic>
#include <libssh2.h>

// Periodically polls the remote host for CPU/load/RAM/swap stats by running
// two exec channels (sampling /proc/stat before and after a 300 ms sleep,
// plus /proc/loadavg and free -b).  All libssh2 calls go through sessionLock
// to stay serialized with the terminal read loop and SFTP workers.
class RemoteStatsWorker : public QThread {
    Q_OBJECT
public:
    explicit RemoteStatsWorker(LIBSSH2_SESSION *session, QMutex *sessionLock,
                               QObject *parent = nullptr);

    void stop();

signals:
    void statsReady(const QJsonObject &stats);

protected:
    void run() override;

private:
    // Opens an exec channel, runs cmd, reads all stdout, closes channel.
    // Returns empty string on failure.
    QString execCommand(const QString &cmd);

    // Parse the two-sample /proc/stat + loadavg + free -b output.
    static QJsonObject parseStats(const QString &combined);

    LIBSSH2_SESSION   *m_session;
    QMutex            *m_sessionLock;
    std::atomic<bool>  m_running { false };
};
