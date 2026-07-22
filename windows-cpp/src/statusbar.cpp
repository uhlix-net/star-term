#include "statusbar.h"

#include <QLabel>

SystemStatusBar::SystemStatusBar(QWidget *parent) : QStatusBar(parent) {
    m_cpuLabel  = new QLabel;
    m_loadLabel = new QLabel;
    m_ramLabel  = new QLabel;
    m_swapLabel = new QLabel;

    for (QLabel *lbl : {m_cpuLabel, m_loadLabel, m_ramLabel, m_swapLabel}) {
        lbl->setMinimumWidth(110);
        addPermanentWidget(lbl);
    }

    updateStats({});
}

void SystemStatusBar::updateStats(const QJsonObject &stats) {
    if (stats.isEmpty()) {
        m_cpuLabel->setText("CPU: N/A");
        m_loadLabel->setText("Load: N/A");
        m_ramLabel->setText("RAM: N/A");
        m_swapLabel->setText("Swap: N/A");
        return;
    }
    if (stats.value("rdp").toBool()) {
        m_cpuLabel->setText( QString("CPU: %1%").arg(stats["cpu"].toInt()));
        m_loadLabel->setText(QString("ProcQ: %1").arg(stats["procq"].toInt()));
        m_ramLabel->setText( QString("RAM: %1%").arg(stats["ram"].toDouble(), 0, 'f', 0));
        m_swapLabel->setText(QString("PF: %1%").arg( stats["pf"].toDouble(),  0, 'f', 0));
        return;
    }
    m_cpuLabel->setText( QString("CPU: %1%").arg(stats["cpu"].toDouble(),  0, 'f', 0));
    m_loadLabel->setText(QString("Load: %1").arg(stats["load"].toDouble(), 0, 'f', 2));
    m_ramLabel->setText( QString("RAM: %1%").arg(stats["ram"].toDouble(),  0, 'f', 0));
    m_swapLabel->setText(QString("Swap: %1%").arg(stats["swap"].toDouble(),0, 'f', 0));
}
