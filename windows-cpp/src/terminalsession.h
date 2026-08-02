#pragma once
#include <QByteArray>
#include <QString>
#include <QThread>

// Common contract for anything that can drive a SessionPane's terminal: an SSH
// channel over libssh2, or a local Windows pseudo-console running a WSL shell.
//
// Subclasses own a read loop in run() and emit dataReceived from that thread;
// send()/resize()/stop() are called from the UI thread and must be safe to call
// concurrently with the read loop.
class TerminalSession : public QThread {
    Q_OBJECT
public:
    explicit TerminalSession(QObject *parent = nullptr) : QThread(parent) {}

    // Called from the UI thread.
    virtual void send(const QByteArray &data) = 0;
    virtual void resize(int cols, int rows)   = 0;

    // Ask the read loop to finish; run() must return promptly afterwards.
    virtual void stop() = 0;

signals:
    void dataReceived(const QByteArray &data);
    void connectionError(const QString &msg);
    void connectionClosed();
    void connected();
};
