#include "statsworker.h"

#include <QMutexLocker>
#include <QStringList>

#include <libssh2.h>

// RAII: switch to blocking mode while lock is held, restore on exit.
// (Same pattern as remotebrowser.cpp's BlockingGuard.)
struct StatsBlockingGuard {
    LIBSSH2_SESSION *s;
    explicit StatsBlockingGuard(LIBSSH2_SESSION *s) : s(s) { libssh2_session_set_blocking(s, 1); }
    ~StatsBlockingGuard() { libssh2_session_set_blocking(s, 0); }
};

// Sample /proc/stat twice (300 ms apart) to compute CPU%, plus load average
// and free -b for RAM/swap.  Split into two exec commands so the 300 ms sleep
// happens in this thread without holding the session mutex.
static const char *STAT_CMD1 = "cat /proc/stat | head -1";
static const char *STAT_CMD2 = "cat /proc/stat | head -1 && cat /proc/loadavg && free -b";

static const int POLL_INTERVAL_MS = 2000;
static const int SLEEP_STEP_MS    = 100;

RemoteStatsWorker::RemoteStatsWorker(LIBSSH2_SESSION *session, QMutex *sessionLock,
                                     QObject *parent)
    : QThread(parent), m_session(session), m_sessionLock(sessionLock)
{}

void RemoteStatsWorker::stop() {
    m_running = false;
}

// -----------------------------------------------------------------------
// execCommand — opens an exec channel, runs cmd, reads all stdout output.
// Non-blocking reads are used so the session mutex is released between
// chunks, allowing the terminal read loop to interleave.
// -----------------------------------------------------------------------
QString RemoteStatsWorker::execCommand(const QString &cmd) {
    LIBSSH2_CHANNEL *chan = nullptr;
    {
        QMutexLocker lock(m_sessionLock);
        StatsBlockingGuard bg(m_session);
        chan = libssh2_channel_open_session(m_session);
        if (chan) {
            QByteArray cmdBytes = cmd.toUtf8();
            if (libssh2_channel_exec(chan, cmdBytes.constData()) != 0) {
                libssh2_channel_free(chan);
                chan = nullptr;
            }
        }
        // BlockingGuard restores non-blocking on exit
    }
    if (!chan) return {};

    QString output;
    char    buf[4096];
    bool    done = false;

    while (!done && m_running) {
        ssize_t n;
        bool    eof = false;
        {
            QMutexLocker lock(m_sessionLock);
            // Session is in non-blocking mode (invariant).
            n   = libssh2_channel_read(chan, buf, sizeof(buf));
            eof = (n == 0) && libssh2_channel_eof(chan);
        }
        if (n > 0) {
            output += QString::fromUtf8(buf, static_cast<int>(n));
        } else if (eof) {
            done = true;
        } else if (n == LIBSSH2_ERROR_EAGAIN || n == 0) {
            msleep(5);
        } else {
            // Unexpected error
            done = true;
        }
    }

    {
        QMutexLocker lock(m_sessionLock);
        StatsBlockingGuard bg(m_session);
        libssh2_channel_close(chan);
        libssh2_channel_free(chan);
    }
    return output;
}

// -----------------------------------------------------------------------
// parseStats — port of Python's _parse_remote_stats().
// combined = first cpu line + "\n" + (second cpu line + loadavg + free -b)
// -----------------------------------------------------------------------
QJsonObject RemoteStatsWorker::parseStats(const QString &combined) {
    QStringList lines;
    for (const QString &l : combined.split('\n'))
        if (!l.trimmed().isEmpty()) lines << l.trimmed();

    if (lines.size() < 6) return {};

    // CPU: parse two /proc/stat samples.
    // Format: "cpu  user nice system idle iowait irq softirq ..."
    auto parseCpuLine = [](const QString &line, long long &total, long long &idle) {
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 5 || parts[0] != "cpu") return false;
        total = 0;
        for (int i = 1; i < parts.size(); ++i)
            total += parts[i].toLongLong();
        idle = parts[4].toLongLong();   // user=1 nice=2 system=3 idle=4
        return true;
    };

    long long total1 = 0, idle1 = 0, total2 = 0, idle2 = 0;
    if (!parseCpuLine(lines[0], total1, idle1)) return {};
    if (!parseCpuLine(lines[1], total2, idle2)) return {};

    long long totalDelta = total2 - total1;
    long long idleDelta  = idle2  - idle1;
    double cpuPct = (totalDelta > 0)
        ? (totalDelta - idleDelta) * 100.0 / totalDelta
        : 0.0;

    // Load: first field of /proc/loadavg.
    QStringList loadFields = lines[2].split(' ', Qt::SkipEmptyParts);
    if (loadFields.isEmpty()) return {};
    bool   ok   = false;
    double load1 = loadFields[0].toDouble(&ok);
    if (!ok) return {};

    // RAM / Swap: last two non-empty lines are the Mem: and Swap: rows from free -b.
    // Format: "Mem:  <total> <used> <free> ..."
    QStringList memFields  = lines[lines.size() - 2].split(' ', Qt::SkipEmptyParts);
    QStringList swapFields = lines[lines.size() - 1].split(' ', Qt::SkipEmptyParts);
    if (memFields.size() < 3 || swapFields.size() < 3) return {};

    long long memTotal  = memFields[1].toLongLong();
    long long memUsed   = memFields[2].toLongLong();
    long long swapTotal = swapFields[1].toLongLong();
    long long swapUsed  = swapFields[2].toLongLong();

    double ramPct  = (memTotal  > 0) ? memUsed  * 100.0 / memTotal  : 0.0;
    double swapPct = (swapTotal > 0) ? swapUsed * 100.0 / swapTotal : 0.0;

    QJsonObject stats;
    stats["cpu"]  = cpuPct;
    stats["load"] = load1;
    stats["ram"]  = ramPct;
    stats["swap"] = swapPct;
    return stats;
}

// -----------------------------------------------------------------------
// run()
// -----------------------------------------------------------------------
void RemoteStatsWorker::run() {
    m_running = true;

    while (m_running) {
        // First CPU sample.
        QString stat1 = execCommand(STAT_CMD1);
        if (stat1.isEmpty() || !m_running) break;

        // Sleep 300 ms between samples without holding the session mutex.
        for (int i = 0; i < 30 && m_running; ++i)
            msleep(10);
        if (!m_running) break;

        // Second CPU sample + loadavg + free -b.
        QString stat2 = execCommand(STAT_CMD2);
        if (stat2.isEmpty() || !m_running) break;

        QJsonObject stats = parseStats(stat1.trimmed() + "\n" + stat2);
        if (!stats.isEmpty())
            emit statsReady(stats);

        // Wait POLL_INTERVAL_MS before next poll.
        for (int i = 0; i < POLL_INTERVAL_MS / SLEEP_STEP_MS && m_running; ++i)
            msleep(SLEEP_STEP_MS);
    }
}
