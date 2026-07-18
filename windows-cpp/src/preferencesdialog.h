#pragma once
#include <QDialog>
#include <QJsonObject>
#include <QString>

class QComboBox;
class QFontComboBox;
class QCheckBox;
class QLineEdit;

// Matches preferences_dialog.py exactly.
class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(
        QWidget *parent = nullptr,
        const QString &fontFamily  = "Courier New",
        int fontSize               = 10,
        const QString &cursorStyle = "underline",
        const QString &themeName   = "dark");

    // Returns {"font_family", "font_size", "cursor_style"}
    QJsonObject getTerminalSettings() const;

    // Returns {"theme", "debug"}
    QJsonObject getGeneralSettings() const;

private slots:
    void browseKey();
    void removeKey();

private:
    QWidget *buildGeneralTab(const QString &themeName);
    QWidget *buildTerminalTab(const QString &fontFamily, int fontSize, const QString &cursorStyle);
    QWidget *buildSSHTab();
    QWidget *buildUpdatesTab();

    QComboBox     *m_themeCombo        = nullptr;
    QCheckBox     *m_debugCheck        = nullptr;
    QFontComboBox *m_fontCombo         = nullptr;
    QComboBox     *m_sizeCombo         = nullptr;
    QComboBox     *m_cursorCombo       = nullptr;
    QLineEdit     *m_keyPathEdit       = nullptr;
    QCheckBox     *m_checkUpdatesCheck = nullptr;
};
