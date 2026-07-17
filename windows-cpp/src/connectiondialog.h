#pragma once
#include <QDialog>
#include <QJsonObject>

class QLineEdit;

// Matches connection_dialog.py exactly.
class ConnectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConnectionDialog(QWidget *parent = nullptr);

    QJsonObject getConnectionParams() const;

private slots:
    void browseKey();

private:
    QLineEdit *m_hostEdit;
    QLineEdit *m_portEdit;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QLineEdit *m_keyPathEdit;
    QLineEdit *m_keyPassphraseEdit;
};
