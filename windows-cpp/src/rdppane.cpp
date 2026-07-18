#include "rdppane.h"

#include <QAxObject>
#include <QAxWidget>
#include <QLabel>
#include <QVBoxLayout>

RdpPane::RdpPane(const QJsonObject &session, QWidget *parent)
    : QWidget(parent)
{
    name   = session.value("name").toString();
    m_host = session.value("host").toString();
    int     port = session.value("port").toInt(3389);
    QString user = session.value("username").toString();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_status = new QLabel(QString("Connecting to %1...").arg(m_host));
    m_status->setAlignment(Qt::AlignCenter);

    m_rdp = new QAxWidget(this);

    // Try progressively older ProgIDs so the control works on Win10 and Win11.
    bool ok = m_rdp->setControl("MsTscAx.MsRdpClient10");
    if (!ok) ok = m_rdp->setControl("MsTscAx.MsRdpClient9Not4K");
    if (!ok) ok = m_rdp->setControl("MsTscAx.MsRdpClient");

    if (!ok) {
        delete m_rdp;
        m_rdp = nullptr;
        m_status->setText(
            "RDP ActiveX control not available on this system.\n"
            "Ensure Remote Desktop Connection (mstscax.dll) is installed.");
        layout->addStretch();
        layout->addWidget(m_status);
        layout->addStretch();
        return;
    }

    m_rdp->setProperty("Server",   m_host);
    m_rdp->setProperty("UserName", user);

    if (port != 3389) {
        QAxObject *adv = m_rdp->querySubObject("AdvancedSettings2");
        if (adv) {
            adv->setProperty("RDPPort", port);
            delete adv;
        }
    }

    // Use old-style SIGNAL/SLOT — ActiveX event names come from the COM
    // type library and are not available as compile-time function pointers.
    connect(m_rdp, SIGNAL(OnConnected()),        this, SLOT(onConnected()));
    connect(m_rdp, SIGNAL(OnDisconnected(long)), this, SLOT(onDisconnected(long)));

    layout->addWidget(m_status);
    layout->addWidget(m_rdp, 1);

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
