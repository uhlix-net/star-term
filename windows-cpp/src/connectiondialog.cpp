#include "connectiondialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

ConnectionDialog::ConnectionDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("New SSH Connection");

    m_hostEdit           = new QLineEdit;
    m_portEdit           = new QLineEdit("22");
    m_usernameEdit       = new QLineEdit;
    m_passwordEdit       = new QLineEdit;
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_keyPathEdit        = new QLineEdit;
    m_keyPassphraseEdit  = new QLineEdit;
    m_keyPassphraseEdit->setEchoMode(QLineEdit::Password);

    QPushButton *browseBtn = new QPushButton("Browse...");
    connect(browseBtn, &QPushButton::clicked, this, &ConnectionDialog::browseKey);

    QWidget *keyRow       = new QWidget;
    QHBoxLayout *keyLayout = new QHBoxLayout(keyRow);
    keyLayout->setContentsMargins(0,0,0,0);
    keyLayout->addWidget(m_keyPathEdit);
    keyLayout->addWidget(browseBtn);

    QFormLayout *form = new QFormLayout(this);
    form->addRow("Host:",             m_hostEdit);
    form->addRow("Port:",             m_portEdit);
    form->addRow("Username:",         m_usernameEdit);
    form->addRow("Password:",         m_passwordEdit);
    form->addRow("Private key file:", keyRow);
    form->addRow("Key passphrase:",   m_keyPassphraseEdit);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addRow(buttons);
}

void ConnectionDialog::browseKey() {
    QString path = QFileDialog::getOpenFileName(this, "Select Private Key");
    if (!path.isEmpty()) m_keyPathEdit->setText(path);
}

QJsonObject ConnectionDialog::getConnectionParams() const {
    QString portText = m_portEdit->text().trimmed();
    bool ok = false;
    int port = portText.toInt(&ok);
    if (!ok) port = 22;

    QJsonObject p;
    p["host"]            = m_hostEdit->text().trimmed();
    p["port"]            = port;
    p["username"]        = m_usernameEdit->text().trimmed();
    p["password"]        = m_passwordEdit->text();
    p["key_path"]        = m_keyPathEdit->text().trimmed();
    p["key_passphrase"]  = m_keyPassphraseEdit->text();
    return p;
}
