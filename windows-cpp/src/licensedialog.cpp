#include "licensedialog.h"
#include "licensing.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

// -----------------------------------------------------------------------
// LicenseStatusWidget
// -----------------------------------------------------------------------
LicenseStatusWidget::LicenseStatusWidget(QWidget *parent) : QWidget(parent) {
    m_statusLabel = new QLabel;
    m_statusLabel->setObjectName("mutedNote");
    m_statusLabel->setWordWrap(true);

    m_keyEdit = new QLineEdit;
    m_keyEdit->setPlaceholderText("Paste license key");
    QPushButton *activateBtn = new QPushButton("Activate");
    connect(activateBtn, &QPushButton::clicked, this, &LicenseStatusWidget::onActivate);

    m_removeBtn = new QPushButton("Remove License");
    connect(m_removeBtn, &QPushButton::clicked, this, &LicenseStatusWidget::onRemove);

    m_errorLabel = new QLabel;
    m_errorLabel->setObjectName("mutedNote");
    m_errorLabel->setWordWrap(true);

    m_keyRow = new QWidget;
    QHBoxLayout *kl = new QHBoxLayout(m_keyRow);
    kl->setContentsMargins(0,0,0,0);
    kl->addWidget(m_keyEdit);
    kl->addWidget(activateBtn);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_keyRow);
    layout->addWidget(m_removeBtn);
    layout->addWidget(m_errorLabel);

    refresh();
}

void LicenseStatusWidget::refresh() {
    LicenseStatus status;
    try {
        status = getLicenseStatus();
    } catch (...) {
        m_errorLabel->setText("Could not read license status.");
        return;
    }
    m_errorLabel->setText("");

    if (status.licensed) {
        QString licensee = status.licenseInfo.value("licensee").toString();
        QString email    = status.licenseInfo.value("email").toString();
        m_statusLabel->setText(QString("Licensed to: %1 (%2)").arg(licensee, email));
        m_keyRow->setVisible(false);
        m_removeBtn->setVisible(true);
    } else {
        m_keyRow->setVisible(true);
        m_removeBtn->setVisible(false);
        if (status.trialExpired) {
            m_statusLabel->setText(
                QString("Your %1-day trial has expired. Enter a license key to continue.")
                    .arg(TRIAL_DAYS));
        } else {
            int days = status.trialDaysRemaining;
            m_statusLabel->setText(
                QString("Trial: %1 day%2 remaining.").arg(days).arg(days != 1 ? "s" : ""));
        }
    }
}

void LicenseStatusWidget::onActivate() {
    QString keyStr = m_keyEdit->text().trimmed();
    if (keyStr.isEmpty()) {
        m_errorLabel->setText("Paste a license key first.");
        return;
    }
    QJsonObject info;
    try {
        info = saveLicenseKey(keyStr);
    } catch (...) {
        m_errorLabel->setText("Could not save license key.");
        return;
    }
    if (info.isEmpty()) {
        m_errorLabel->setText("Invalid license key.");
        return;
    }
    m_keyEdit->clear();
    refresh();
    emit activated();
}

void LicenseStatusWidget::onRemove() {
    clearLicenseKey();
    refresh();
}

// -----------------------------------------------------------------------
// LicenseDialog
// -----------------------------------------------------------------------
LicenseDialog::LicenseDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("License");

    LicenseStatusWidget *sw = new LicenseStatusWidget;
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(sw);
    layout->addWidget(buttons);
}

// -----------------------------------------------------------------------
// LicenseGateDialog
// -----------------------------------------------------------------------
LicenseGateDialog::LicenseGateDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Star Term - License Required");

    QLabel *intro = new QLabel(
        QString("Your %1-day trial of Star Term has expired.\n"
                "Enter a license key to continue, or exit.")
            .arg(TRIAL_DAYS));
    intro->setWordWrap(true);

    LicenseStatusWidget *sw = new LicenseStatusWidget;
    connect(sw, &LicenseStatusWidget::activated, this, &QDialog::accept);

    QPushButton *exitBtn = new QPushButton("Exit");
    connect(exitBtn, &QPushButton::clicked, this, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(sw);
    layout->addWidget(exitBtn);
}
