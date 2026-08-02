#pragma once
#include "terminalsession.h"

#include <QMutex>
#include <QStringList>

#include <atomic>

// -----------------------------------------------------------------------
// WSL runtime helpers.
//
// All of these are deliberately locale-independent: `wsl --list --verbose`
// prints a localized STATE column and `wsl --list` marks the default with a
// translated "(Default)" suffix, so neither can be parsed reliably.  The quiet
// listings emit bare distribution names, and the default is read from the
// registry.  wsl.exe writes its output as UTF-16LE.
// -----------------------------------------------------------------------
QStringList wslDistributions();          // every installed distribution
QStringList wslRunningDistributions();   // those currently running
QString     wslDefaultDistribution();    // "" if it cannot be determined

// Boots a stopped distribution. Blocks until it is up (or the timeout expires).
bool wslStartDistribution(const QString &distro, QString *error = nullptr);

// -----------------------------------------------------------------------
// A WSL shell running in a Windows pseudo-console (ConPTY).
// -----------------------------------------------------------------------
class WslSession : public TerminalSession {
    Q_OBJECT
public:
    explicit WslSession(const QString &distro, int cols = 80, int rows = 24,
                        QObject *parent = nullptr);
    ~WslSession() override;

    void send(const QByteArray &data) override;
    void resize(int cols, int rows) override;
    void stop() override;

protected:
    void run() override;

private:
    bool openPty(QString *error);
    void closeHandles();

    QString           m_distro;
    int               m_cols;
    int               m_rows;
    std::atomic<bool> m_running { false };

    // Guards the handles below.  Raw void* so windows.h stays out of headers.
    QMutex m_ioMutex;
    void  *m_hPC     = nullptr;   // HPCON
    void  *m_inWrite = nullptr;   // us -> shell stdin
    void  *m_outRead = nullptr;   // shell stdout -> us
    void  *m_process = nullptr;   // child process handle
};
