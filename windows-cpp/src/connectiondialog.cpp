#include "connectiondialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

static const int SSH_PORT = 22;
static const int RDP_PORT = 3389;

ConnectionDialog::ConnectionDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("New Connection");

    m_typeCombo = new QComboBox;
    m_typeCombo->addItem("SSH", "ssh");
    m_typeCombo->addItem("RDP", "rdp");

    m_hostEdit           = new QLineEdit;
    m_portEdit           = new QLineEdit(QString::number(SSH_PORT));
    m_usernameEdit       = new QLineEdit;
    m_keyPathEdit        = new QLineEdit;
    m_keyPassphraseEdit  = new QLineEdit;
    m_keyPassphraseEdit->setEchoMode(QLineEdit::Password);

    QPushButton *browseBtn = new QPushButton("Browse...");
    connect(browseBtn, &QPushButton::clicked, this, &ConnectionDialog::browseKey);

    m_keyRow = new QWidget;
    QHBoxLayout *keyLayout = new QHBoxLayout(m_keyRow);
    keyLayout->setContentsMargins(0,0,0,0);
    keyLayout->addWidget(m_keyPathEdit);
    keyLayout->addWidget(browseBtn);

    m_form = new QFormLayout(this);
    m_form->addRow("Connection type:", m_typeCombo);
    m_form->addRow("Host:",            m_hostEdit);
    m_form->addRow("Port:",            m_portEdit);
    m_form->addRow("Username:",        m_usernameEdit);
    m_form->addRow("Private key file:", m_keyRow);
    m_form->addRow("Key passphrase:",  m_keyPassphraseEdit);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_form->addRow(buttons);

    connect(m_typeCombo, &QComboBox::currentIndexChanged,
            this, &ConnectionDialog::onTypeChanged);
    onTypeChanged();
}

bool ConnectionDialog::isRdp() const {
    return m_typeCombo->currentData().toString() == QLatin1String("rdp");
}

void ConnectionDialog::onTypeChanged() {
    const bool rdp = isRdp();

    // Move the port to the new protocol's default, but keep a port the user has
    // deliberately customised.
    const int     previousDefault = rdp ? SSH_PORT : RDP_PORT;
    const QString port            = m_portEdit->text().trimmed();
    if (port.isEmpty() || port == QString::number(previousDefault))
        m_portEdit->setText(QString::number(rdp ? RDP_PORT : SSH_PORT));

    // Key authentication is SSH-only; RDP authenticates with the account password.
    m_form->setRowVisible(m_keyRow,            !rdp);
    m_form->setRowVisible(m_keyPassphraseEdit, !rdp);
}

void ConnectionDialog::browseKey() {
    QString path = QFileDialog::getOpenFileName(this, "Select Private Key");
    if (!path.isEmpty()) m_keyPathEdit->setText(path);
}

QJsonObject ConnectionDialog::getConnectionParams() const {
    const bool rdp = isRdp();

    bool ok   = false;
    int  port = m_portEdit->text().trimmed().toInt(&ok);
    if (!ok) port = rdp ? RDP_PORT : SSH_PORT;

    QJsonObject p;
    p["type"]     = rdp ? QStringLiteral("rdp") : QStringLiteral("ssh");
    p["host"]     = m_hostEdit->text().trimmed();
    p["port"]     = port;
    p["username"] = m_usernameEdit->text().trimmed();
    if (!rdp) {
        p["key_path"]       = m_keyPathEdit->text().trimmed();
        p["key_passphrase"] = m_keyPassphraseEdit->text();
    }
    return p;
}
