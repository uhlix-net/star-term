#pragma once
#include <QDialog>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

// Matches license_dialog.py exactly.

class LicenseStatusWidget : public QWidget {
    Q_OBJECT
public:
    explicit LicenseStatusWidget(QWidget *parent = nullptr);
    void refresh();

signals:
    void activated();

private slots:
    void onActivate();
    void onRemove();

private:
    QLabel   *m_statusLabel = nullptr;
    QLabel   *m_errorLabel  = nullptr;
    QLineEdit *m_keyEdit    = nullptr;
    QWidget  *m_keyRow      = nullptr;
    QPushButton *m_removeBtn = nullptr;
};

class LicenseDialog : public QDialog {
    Q_OBJECT
public:
    explicit LicenseDialog(QWidget *parent = nullptr);
};

class LicenseGateDialog : public QDialog {
    Q_OBJECT
public:
    explicit LicenseGateDialog(QWidget *parent = nullptr);
};
