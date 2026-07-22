#pragma once
#include <QStatusBar>
#include <QJsonObject>

#include "licensing.h"

class QLabel;

class SystemStatusBar : public QStatusBar {
    Q_OBJECT
public:
    explicit SystemStatusBar(QWidget *parent = nullptr);
    void updateStats(const QJsonObject &stats);
    void setLicenseStatus(const LicenseStatus &status);

private:
    QLabel *m_licenseLabel = nullptr;
    QLabel *m_cpuLabel     = nullptr;
    QLabel *m_loadLabel    = nullptr;
    QLabel *m_ramLabel     = nullptr;
    QLabel *m_swapLabel    = nullptr;
};
