#pragma once
#include <QStatusBar>
#include <QJsonObject>

class QLabel;

// Matches status_bar.py exactly.
class SystemStatusBar : public QStatusBar {
    Q_OBJECT
public:
    explicit SystemStatusBar(QWidget *parent = nullptr);
    void updateStats(const QJsonObject &stats);

private:
    QLabel *m_cpuLabel  = nullptr;
    QLabel *m_loadLabel = nullptr;
    QLabel *m_ramLabel  = nullptr;
    QLabel *m_swapLabel = nullptr;
};
