#pragma once
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

#include "secretstore.h"

// The one dialog for describing a session — host, credentials, SSH key and its
// passphrase. Shared by the Sessions pane (Add/Edit a saved session) and by
// File > Connect, so the two cannot drift apart again.
//
// Mode only changes the window title and whether the "Save to Sessions"
// checkbox appears; the fields are the same either way.

class SessionEditDialog : public QDialog {
public:
    enum Mode {
        EditSaved,    // Sessions pane: the result is written to sessions.json
        ConnectNow    // File > Connect: connect, and save only if asked to
    };

    SessionEditDialog(QWidget *parent, const QJsonObject &session,
                      const QStringList &folders, Mode mode = EditSaved)
        : QDialog(parent), m_mode(mode)
    {
        setWindowTitle(mode == ConnectNow ? "Connect" : "Session");

        QString type = session.value("type").toString("ssh");

        m_nameEdit   = new QLineEdit(session.value("name").toString());
        m_folderEdit = new QComboBox;
        m_folderEdit->setEditable(true);
        m_folderEdit->addItems(folders);
        m_folderEdit->setCurrentText(session.value("folder").toString());
        m_folderEdit->lineEdit()->setPlaceholderText("(no folder)");

        m_typeCombo = new QComboBox;
        m_typeCombo->addItems({"SSH", "RDP"});
        m_typeCombo->setCurrentText(type.toUpper());

        m_hostEdit     = new QLineEdit(session.value("host").toString());
        int defaultPort = (type == "rdp") ? 3389 : 22;
        m_portEdit     = new QLineEdit(QString::number(session.value("port").toInt(defaultPort)));
        m_usernameEdit = new QLineEdit(session.value("username").toString());

        // SSH-only group
        m_sshGroup = new QGroupBox("SSH Options");
        m_useKeyCheck  = new QCheckBox("Use SSH key authentication");
        m_useKeyCheck->setChecked(session.value("use_key").toBool(false));
        m_keyPathEdit  = new QLineEdit(session.value("key_path").toString());
        m_keyPathEdit->setPlaceholderText("(uses Settings key)");
        m_keyPathEdit->setMinimumWidth(160);

        QPushButton *browseBtn = new QPushButton("Browse...");
        connect(browseBtn, &QPushButton::clicked, this, [this]() {
            QString p = QFileDialog::getOpenFileName(this, "Select Private Key");
            if (!p.isEmpty()) m_keyPathEdit->setText(p);
        });
        QPushButton *clearBtn = new QPushButton("Clear");
        connect(clearBtn, &QPushButton::clicked, m_keyPathEdit, &QLineEdit::clear);

        QWidget *keyRow = new QWidget;
        QHBoxLayout *kl = new QHBoxLayout(keyRow);
        kl->setContentsMargins(0,0,0,0);
        kl->addWidget(m_keyPathEdit);
        kl->addWidget(browseBtn);
        kl->addWidget(clearBtn);

        // Passphrase for the key, stored with the session so connecting does not
        // stop to ask. Hidden by default; the toggle reveals it for checking a
        // typo. Held encrypted at rest — see SecretStore.
        m_keyPassEdit = new QLineEdit(
            SecretStore::unprotect(session.value("key_passphrase_enc").toString()));
        m_keyPassEdit->setEchoMode(QLineEdit::Password);
        m_keyPassEdit->setPlaceholderText("(key has no passphrase)");
        m_keyPassEdit->setMinimumWidth(160);

        QPushButton *revealBtn = new QPushButton("Show");
        revealBtn->setCheckable(true);
        revealBtn->setToolTip("Show or hide the passphrase");
        connect(revealBtn, &QPushButton::toggled, this, [this, revealBtn](bool shown) {
            m_keyPassEdit->setEchoMode(shown ? QLineEdit::Normal : QLineEdit::Password);
            revealBtn->setText(shown ? "Hide" : "Show");
        });

        QWidget *passRow = new QWidget;
        QHBoxLayout *pl = new QHBoxLayout(passRow);
        pl->setContentsMargins(0,0,0,0);
        pl->addWidget(m_keyPassEdit);
        pl->addWidget(revealBtn);

        QFormLayout *sshForm = new QFormLayout(m_sshGroup);
        sshForm->addRow(m_useKeyCheck);
        sshForm->addRow("SSH key override:", keyRow);
        sshForm->addRow("Key passphrase:", passRow);

        QFormLayout *form = new QFormLayout;
        form->addRow("Name:",    m_nameEdit);
        form->addRow("Folder:",  m_folderEdit);
        form->addRow("Type:",    m_typeCombo);
        form->addRow("Host:",    m_hostEdit);
        form->addRow("Port:",    m_portEdit);
        form->addRow("Username:", m_usernameEdit);

        QDialogButtonBox *buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        QVBoxLayout *root = new QVBoxLayout(this);
        root->addLayout(form);
        root->addWidget(m_sshGroup);

        // A connection started from File > Connect is one-off unless the user
        // says otherwise; editing a saved session is always a save.
        if (m_mode == ConnectNow) {
            m_saveCheck = new QCheckBox("Save to Sessions");
            m_saveCheck->setToolTip(
                "Add this to the Sessions pane, in the folder selected above");
            root->addWidget(m_saveCheck);
        }

        root->addWidget(buttons);

        connect(m_typeCombo, &QComboBox::currentTextChanged,
                this, &SessionEditDialog::onTypeChanged);
        onTypeChanged(m_typeCombo->currentText());
    }

    // True when File > Connect should also store this session. Always false
    // in EditSaved mode, where the caller saves unconditionally.
    bool saveRequested() const { return m_saveCheck && m_saveCheck->isChecked(); }

    QJsonObject getSession() const {
        QString typeStr = m_typeCombo->currentText().toLower(); // "ssh" or "rdp"
        int defaultPort = (typeStr == "rdp") ? 3389 : 22;
        QString portText = m_portEdit->text().trimmed();
        bool ok = false;
        int port = portText.toInt(&ok);
        if (!ok) port = defaultPort;

        QString host = m_hostEdit->text().trimmed();
        QString name = m_nameEdit->text().trimmed();
        if (name.isEmpty()) name = host;

        QJsonObject s;
        s["type"]     = typeStr;
        s["name"]     = name;
        s["folder"]   = m_folderEdit->currentText().trimmed();
        s["host"]     = host;
        s["port"]     = port;
        s["username"] = m_usernameEdit->text().trimmed();
        s["use_key"]  = m_useKeyCheck->isChecked();
        s["key_path"] = m_keyPathEdit->text().trimmed();
        // Never written in clear text. An empty passphrase stores nothing.
        s["key_passphrase_enc"] = SecretStore::protect(m_keyPassEdit->text());
        return s;
    }

private slots:
    void onTypeChanged(const QString &text) {
        bool isSsh = (text == "SSH");
        m_sshGroup->setVisible(isSsh);
        // Flip port default when switching types
        int curPort = m_portEdit->text().toInt();
        if (!isSsh && curPort == 22)   m_portEdit->setText("3389");
        if (isSsh  && curPort == 3389) m_portEdit->setText("22");
        adjustSize();
    }

private:
    Mode        m_mode;
    QCheckBox  *m_saveCheck = nullptr;
    QLineEdit  *m_nameEdit, *m_hostEdit, *m_portEdit, *m_usernameEdit, *m_keyPathEdit;
    QLineEdit  *m_keyPassEdit;
    QComboBox  *m_folderEdit, *m_typeCombo;
    QCheckBox  *m_useKeyCheck;
    QGroupBox  *m_sshGroup;
};
