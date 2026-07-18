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

    QLabel *titleLabel = new QLabel(name);
    titleLabel->setObjectName("sectionTitle");

    terminal = new TerminalWidget(80, 24, fontFamily, fontSize, cursorStyle, this);

    excludeCheckbox = new QCheckBox("Exclude from Multi-Exec", this);

    reconnectBtn = new QPushButton("Reconnect", this);
    reconnectBtn->setVisible(false);
    connect(reconnectBtn, &QPushButton::clicked, this, &SessionPane::reconnectRequested);

    QPushButton *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &SessionPane::closeRequested);

    QWidget *controls = new QWidget(this);
    QHBoxLayout *ctrlLayout = new QHBoxLayout(controls);
    ctrlLayout->setContentsMargins(0, 0, 0, 0);
    ctrlLayout->setSpacing(8);
    ctrlLayout->addWidget(excludeCheckbox);
    ctrlLayout->addStretch();
    ctrlLayout->addWidget(reconnectBtn);
    ctrlLayout->addWidget(closeBtn);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);
    layout->addWidget(titleLabel);
    layout->addWidget(terminal, 1);
    layout->addWidget(controls);

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

void SessionPane::startStatsWorker() {
    if (!session || m_statsWorker) return;
    m_statsWorker = new RemoteStatsWorker(
        session->rawSession(), session->sessionLock(), this);
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
