#include "wsldialog.h"
#include "wslsession.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>

WslConnectDialog::WslConnectDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Connect to WSL Distribution");

    const QStringList all       = wslDistributions();
    const QStringList running   = wslRunningDistributions();
    const QString     defaultOne = wslDefaultDistribution();

    m_hasDistributions = !all.isEmpty();

    m_distroCombo = new QComboBox;
    for (const QString &distro : all) {
        // The bare name is kept as item data; the label is decorated only for
        // display, so selectedDistribution() never has to strip it back off.
        m_distroCombo->addItem(
            QString("%1  (%2)").arg(distro,
                running.contains(distro) ? "running" : "stopped"),
            distro);
    }

    if (!defaultOne.isEmpty()) {
        const int idx = m_distroCombo->findData(defaultOne);
        if (idx >= 0) m_distroCombo->setCurrentIndex(idx);
    }

    QFormLayout *form = new QFormLayout(this);
    form->addRow("Distribution:", m_distroCombo);

    QLabel *note = new QLabel(
        "A stopped distribution will be started before the shell opens.");
    note->setObjectName("mutedNote");
    note->setWordWrap(true);
    form->addRow(note);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addRow(buttons);

    if (!m_hasDistributions)
        buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
}

QString WslConnectDialog::selectedDistribution() const {
    return m_distroCombo->currentData().toString();
}
