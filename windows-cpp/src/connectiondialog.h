#pragma once
#include <QDialog>
#include <QJsonObject>

class QComboBox;
class QFormLayout;
class QLineEdit;

// Ad-hoc connection dialog for File > Connect and the toolbar Connect button.
// Collects an SSH or RDP target; no password is gathered here — SSH prompts for
// one at connect time and RDP passes credentials straight to the ActiveX control.
class ConnectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConnectionDialog(QWidget *parent = nullptr);

    QJsonObject getConnectionParams() const;

private slots:
    void browseKey();
    void onTypeChanged();

private:
    bool isRdp() const;

    QFormLayout *m_form;
    QComboBox   *m_typeCombo;
    QLineEdit   *m_hostEdit;
    QLineEdit   *m_portEdit;
    QLineEdit   *m_usernameEdit;
    QLineEdit   *m_keyPathEdit;
    QLineEdit   *m_keyPassphraseEdit;
    QWidget     *m_keyRow;
};
