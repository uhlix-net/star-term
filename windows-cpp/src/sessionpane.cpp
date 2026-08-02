#include "sessionpane.h"
#include "statsworker.h"
#include "terminalwidget.h"
#include "sshsession.h"
#include "remotebrowser.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SessionPane::SessionPane(
    const QString &name_,
    const QString &fontFamily,
    int fontSize,
    const QString &cursorStyle,
    QWidget *parent)
    : QWidget(parent)
    , name(name_)
{
    cwdTracker = new CwdTracker(this);

    terminal = new TerminalWidget(80, 24, fontFamily, fontSize, cursorStyle, this);

    // Shown only while Multi-Exec is active — it means nothing otherwise.
    excludeCheckbox = new QCheckBox("Exclude from Multi-Exec", this);
    excludeCheckbox->setVisible(false);

    // Offered only after a session drops.
    reconnectBtn = new QPushButton("Reconnect", this);
    reconnectBtn->setVisible(false);
    connect(reconnectBtn, &QPushButton::clicked, this, &SessionPane::reconnectRequested);

    // No title label and no Close button: the tab already carries the session
    // name and a close affordance, so repeating them inside the pane is just
    // chrome around the terminal.
    m_controls = new QWidget(this);
    QHBoxLayout *ctrlLayout = new QHBoxLayout(m_controls);
    ctrlLayout->setContentsMargins(6, 4, 6, 4);
    ctrlLayout->setSpacing(8);
    ctrlLayout->addWidget(excludeCheckbox);
    ctrlLayout->addStretch();
    ctrlLayout->addWidget(reconnectBtn);
    m_controls->setVisible(false);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(terminal, 1);
    layout->addWidget(m_controls);

    connect(terminal, &TerminalWidget::dataToSend, this, &SessionPane::dataToSend);
    connect(terminal, &TerminalWidget::dataToSend, cwdTracker, &CwdTracker::feedInput);
    connect(terminal, &TerminalWidget::sizeChanged, this, &SessionPane::sizeChanged);
}

SessionPane::~SessionPane() {
    disconnectSession();
}

void SessionPane::applySettings(const QString &fontFamily, int fontSize, const QString &cursorStyle) {
    terminal->applySettings(fontFamily, fontSize, cursorStyle);
}

void SessionPane::setReconnectVisible(bool on) {
    reconnectBtn->setVisible(on);
    updateControlsVisibility();
}

void SessionPane::setMultiExecControlsVisible(bool on) {
    excludeCheckbox->setVisible(on);
    updateControlsVisibility();
}

void SessionPane::updateControlsVisibility() {
    m_controls->setVisible(reconnectBtn->isVisible() || excludeCheckbox->isVisible());
}

void SessionPane::startStatsWorker() {
    if (m_statsWorker) return;
    // Stats are gathered by running commands over the SSH channel, so there is
    // nothing to poll for a local WSL session.
    SSHSession *ssh = qobject_cast<SSHSession*>(session);
    if (!ssh) return;
    m_statsWorker = new RemoteStatsWorker(
        ssh->rawSession(), ssh->sessionLock(), this);
    connect(m_statsWorker, &RemoteStatsWorker::statsReady,
            this, [this](const QJsonObject &stats) {
        lastStats = stats;
        emit statsUpdated(stats);
    });
    m_statsWorker->start();
}

void SessionPane::stopStatsWorker() {
    if (!m_statsWorker) return;
    m_statsWorker->stop();
    if (!m_statsWorker->wait(3000))
        m_statsWorker->terminate();
    m_statsWorker->wait(1000);
    delete m_statsWorker;
    m_statsWorker = nullptr;
    lastStats = {};
}

void SessionPane::disconnectSession() {
    stopStatsWorker();
    if (session) {
        session->stop();
        if (!session->wait(5000)) {
            session->terminate();
            session->wait(2000);
        }
        session->deleteLater();
        session = nullptr;
    }
}
