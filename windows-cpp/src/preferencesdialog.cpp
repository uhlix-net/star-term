#include "preferencesdialog.h"
#include "config.h"
#include "debug.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFont>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

PreferencesDialog::PreferencesDialog(
    QWidget *parent,
    const QString &fontFamily,
    int fontSize,
    const QString &cursorStyle,
    const QString &themeName)
    : QDialog(parent)
{
    setWindowTitle("Preferences");

    QTabWidget *tabs = new QTabWidget;
    tabs->addTab(buildGeneralTab(themeName),                          "General");
    tabs->addTab(buildTerminalTab(fontFamily, fontSize, cursorStyle), "Terminal");
    tabs->addTab(buildSSHTab(),                                       "SSH Key");
    tabs->addTab(buildUpdatesTab(),                                   "Updates");
    tabs->addTab(buildRdpTab(),                                       "RDP");

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
}

// -----------------------------------------------------------------------
QWidget *PreferencesDialog::buildGeneralTab(const QString &themeName) {
    QWidget *w = new QWidget;

    m_themeCombo = new QComboBox;
    m_themeCombo->addItems({"Dark", "Light"});
    m_themeCombo->setCurrentText(themeName == "light" ? "Light" : "Dark");

    QJsonObject settings = loadSettings();
    m_debugCheck = new QCheckBox("Enable debug logging");
    m_debugCheck->setChecked(settings.value("debug").toBool(false));

    QLabel *debugNote = new QLabel(QString("Log file: %1").arg(debugGetLogPath()));
    debugNote->setObjectName("mutedNote");
    debugNote->setWordWrap(true);

    QFormLayout *form = new QFormLayout(w);
    form->addRow("Appearance:", m_themeCombo);
    form->addRow(m_debugCheck);
    form->addRow(debugNote);
    return w;
}

QJsonObject PreferencesDialog::getGeneralSettings() const {
    QJsonObject s;
    s["theme"]           = m_themeCombo->currentText().toLower();
    s["debug"]           = m_debugCheck->isChecked();
    s["check_updates"]   = m_checkUpdatesCheck->isChecked();
    s["rdp_resize_mode"] = m_rdpScaleRadio->isChecked() ? QString("scale") : QString("scroll");
    return s;
}

// -----------------------------------------------------------------------
QWidget *PreferencesDialog::buildSSHTab() {
    QWidget *w = new QWidget;
    QJsonObject settings = loadSettings();

    m_keyPathEdit = new QLineEdit(settings.value("ssh_key_path").toString());
    m_keyPathEdit->setReadOnly(true);

    QPushButton *browseBtn = new QPushButton("Browse...");
    connect(browseBtn, &QPushButton::clicked, this, &PreferencesDialog::browseKey);
    QPushButton *removeBtn = new QPushButton("Remove");
    connect(removeBtn, &QPushButton::clicked, this, &PreferencesDialog::removeKey);

    QWidget *keyRow = new QWidget;
    QHBoxLayout *kl = new QHBoxLayout(keyRow);
    kl->setContentsMargins(0,0,0,0);
    kl->addWidget(m_keyPathEdit);
    kl->addWidget(browseBtn);
    kl->addWidget(removeBtn);

    QLabel *note = new QLabel("This option can be over-ridden on the session profile.");
    note->setObjectName("mutedNote");

    QFormLayout *form = new QFormLayout(w);
    form->addRow("Default SSH key:", keyRow);
    form->addRow(note);
    return w;
}

void PreferencesDialog::browseKey() {
    QString path = QFileDialog::getOpenFileName(this, "Select SSH Private Key");
    if (path.isEmpty()) return;
    QJsonObject settings = loadSettings();
    settings["ssh_key_path"] = path;
    saveSettings(settings);
    m_keyPathEdit->setText(path);
}

void PreferencesDialog::removeKey() {
    QJsonObject settings = loadSettings();
    settings.remove("ssh_key_path");
    saveSettings(settings);
    m_keyPathEdit->clear();
}

// -----------------------------------------------------------------------
QWidget *PreferencesDialog::buildUpdatesTab() {
    QWidget *w = new QWidget;
    QJsonObject settings = loadSettings();

    m_checkUpdatesCheck = new QCheckBox("Check for updates on startup");
    m_checkUpdatesCheck->setChecked(settings.value("check_updates").toBool(true));

    QLabel *note = new QLabel(
        "When enabled, Star Term checks GitHub releases on launch\n"
        "and notifies you if a newer version is available.");
    note->setObjectName("mutedNote");
    note->setWordWrap(true);

    QVBoxLayout *layout = new QVBoxLayout(w);
    layout->addWidget(m_checkUpdatesCheck);
    layout->addWidget(note);
    layout->addStretch();
    return w;
}

// -----------------------------------------------------------------------
QWidget *PreferencesDialog::buildTerminalTab(
    const QString &fontFamily, int fontSize, const QString &cursorStyle)
{
    QWidget *w = new QWidget;

    m_fontCombo = new QFontComboBox;
    m_fontCombo->setCurrentFont(QFont(fontFamily));

    m_sizeCombo = new QComboBox;
    m_sizeCombo->setEditable(true);
    QStringList sizes;
    for (int s : {6,7,8,9,10,11,12,14,16,18,20,22,24,28,32,36})
        sizes << QString::number(s);
    m_sizeCombo->addItems(sizes);
    m_sizeCombo->setValidator(new QIntValidator(6, 72, m_sizeCombo));
    m_sizeCombo->setCurrentText(QString::number(fontSize));

    m_cursorCombo = new QComboBox;
    m_cursorCombo->addItems({"Underline", "Block"});
    m_cursorCombo->setCurrentText(cursorStyle == "block" ? "Block" : "Underline");

    QFormLayout *form = new QFormLayout(w);
    form->addRow("Font:",         m_fontCombo);
    form->addRow("Size:",         m_sizeCombo);
    form->addRow("Cursor style:", m_cursorCombo);
    return w;
}

QJsonObject PreferencesDialog::getTerminalSettings() const {
    bool ok = false;
    int sz = m_sizeCombo->currentText().toInt(&ok);
    if (!ok) sz = 10;

    QJsonObject s;
    s["font_family"]  = m_fontCombo->currentFont().family();
    s["font_size"]    = sz;
    s["cursor_style"] = m_cursorCombo->currentText().toLower();
    return s;
}

// -----------------------------------------------------------------------
QWidget *PreferencesDialog::buildRdpTab() {
    QWidget *w = new QWidget;
    QJsonObject settings = loadSettings();
    QString mode = settings.value("rdp_resize_mode").toString("scroll");

    m_rdpScaleRadio  = new QRadioButton("Scale to fit");
    m_rdpScrollRadio = new QRadioButton("Fixed resolution with scroll bars");

    if (mode == "scale") m_rdpScaleRadio->setChecked(true);
    else                 m_rdpScrollRadio->setChecked(true);

    QLabel *scaleNote = new QLabel(
        "The mstsc window tracks the tab size. The session resolution\n"
        "is set at connect time; content scrolls if the tab is made smaller.");
    scaleNote->setObjectName("mutedNote");
    scaleNote->setWordWrap(true);

    QLabel *scrollNote = new QLabel(
        "Session resolution is fixed at connect time. Qt scroll bars\n"
        "appear when the tab is made smaller than the session.");
    scrollNote->setObjectName("mutedNote");
    scrollNote->setWordWrap(true);

    QVBoxLayout *layout = new QVBoxLayout(w);
    layout->addWidget(new QLabel("Window resize behavior:"));
    layout->addSpacing(4);
    layout->addWidget(m_rdpScaleRadio);
    layout->addWidget(scaleNote);
    layout->addSpacing(8);
    layout->addWidget(m_rdpScrollRadio);
    layout->addWidget(scrollNote);
    layout->addStretch();
    return w;
}
