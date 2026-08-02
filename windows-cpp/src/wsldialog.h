#pragma once
#include <QDialog>
#include <QString>

class QComboBox;

// Picks a WSL distribution to open a shell in. Distributions are enumerated on
// construction; the default distribution is preselected and each entry shows
// whether it is currently running.
class WslConnectDialog : public QDialog {
    Q_OBJECT
public:
    explicit WslConnectDialog(QWidget *parent = nullptr);

    QString selectedDistribution() const;

    // False when no distributions are installed — the caller should say so
    // rather than showing an empty dialog.
    bool hasDistributions() const { return m_hasDistributions; }

private:
    QComboBox *m_distroCombo   = nullptr;
    bool       m_hasDistributions = false;
};
