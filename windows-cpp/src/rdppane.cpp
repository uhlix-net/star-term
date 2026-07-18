#include "rdppane.h"

#include <QAxObject>
#include <QAxWidget>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

RdpPane::RdpPane(const QJsonObject &session, QWidget *parent)
    : QWidget(parent)
{
    name   = session.value("name").toString();
    m_host = session.value("host").toString();
    m_port = session.value("port").toInt(3389);
    m_user = session.value("username").toString();

    // Force a native Win32 HWND so the ActiveX control has a real window to
    // embed into. Without this, setControl() can silently fail on Qt6.
    setAttribute(Qt::WA_NativeWindow);

    m_status = new QLabel(QString("Connecting to %1...").arg(m_host));
    m_status->setAlignment(Qt::AlignCenter);

    m_rdp = new QAxWidget(this);
    m_rdp->setAttribute(Qt::WA_NativeWindow);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_status);
    layout->addWidget(m_rdp, 1);
    // ActiveX init deferred to showEvent — widget must be visible first.
}

void RdpPane::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (!m_initialized) {
        m_initialized = true;
        // Use singleShot(0) so Qt finishes painting the tab before we block
        // on COM initialization.
        QTimer::singleShot(0, this, &RdpPane::initRdp);
    }
}

void RdpPane::initRdp() {
    // Try progressively older ProgIDs; fall back to versionless alias.
    bool ok = m_rdp->setControl("MsTscAx.MsRdpClient10");
    if (!ok) ok = m_rdp->setControl("MsTscAx.MsRdpClient9Not4K");
    if (!ok) ok = m_rdp->setControl("MsTscAx.MsRdpClient8");
    if (!ok) ok = m_rdp->setControl("MsTscAx.MsRdpClient");

    if (!ok) {
        m_rdp->hide();
        m_status->setText(
            "RDP ActiveX control could not be loaded.\n"
            "Ensure the Qt6 ActiveQt module is installed and mstscax.dll is registered.");
        m_status->show();
        return;
    }

    m_rdp->setProperty("Server",   m_host);
    m_rdp->setProperty("UserName", m_user);

    if (m_port != 3389) {
        QAxObject *adv = m_rdp->querySubObject("AdvancedSettings2");
        if (adv) {
            adv->setProperty("RDPPort", m_port);
            delete adv;
        }
    }

    // Use old-style SIGNAL/SLOT — ActiveX event names come from the COM
    // type library and are not available as compile-time function pointers.
    connect(m_rdp, SIGNAL(OnConnected()),        this, SLOT(onConnected()));
    connect(m_rdp, SIGNAL(OnDisconnected(long)), this, SLOT(onDisconnected(long)));

    m_rdp->dynamicCall("Connect()");
}

void RdpPane::disconnectRdp() {
    if (m_rdp) m_rdp->dynamicCall("Disconnect()");
}

void RdpPane::onConnected() {
    m_status->hide();
}

void RdpPane::onDisconnected(long reason) {
    if (!m_rdp) return;
    // reason == 1 means we called Disconnect() ourselves (tab close) — stay quiet.
    if (reason == 1) return;
    m_rdp->hide();
    m_status->setText(reason == 2
        ? QString("Session ended by %1.").arg(m_host)
        : QString("Disconnected from %1.").arg(m_host));
    m_status->show();
}
