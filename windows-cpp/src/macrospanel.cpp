#include "macrospanel.h"
#include "config.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

// -----------------------------------------------------------------------
// MacroEditDialog (inner)
// -----------------------------------------------------------------------
class MacroEditDialog : public QDialog {
public:
    MacroEditDialog(QWidget *parent = nullptr, const QJsonObject &macro = {})
        : QDialog(parent)
    {
        setWindowTitle("Macro");

        m_nameEdit = new QLineEdit(macro.value("name").toString());
        m_commandsEdit = new QPlainTextEdit(macro.value("commands").toString());
        m_commandsEdit->setPlaceholderText(
            "Commands/keystrokes to send, one per line.\n"
            "Each line but the last is sent followed by Enter.");
        m_commandsEdit->setMinimumSize(360, 160);

        m_autoExecCheck = new QCheckBox(
            "Press Enter after the last line (execute immediately)");
        m_autoExecCheck->setChecked(macro.value("auto_execute").toBool(false));

        QFormLayout *form = new QFormLayout;
        form->addRow("Name:", m_nameEdit);
        form->addRow("Commands:", m_commandsEdit);
        form->addRow(m_autoExecCheck);

        QDialogButtonBox *buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
    }

    QJsonObject getMacro() const {
        QJsonObject m;
        QString name = m_nameEdit->text().trimmed();
        m["name"]         = name.isEmpty() ? "Macro" : name;
        m["commands"]     = m_commandsEdit->toPlainText();
        m["auto_execute"] = m_autoExecCheck->isChecked();
        return m;
    }

private:
    QLineEdit     *m_nameEdit      = nullptr;
    QPlainTextEdit *m_commandsEdit = nullptr;
    QCheckBox     *m_autoExecCheck = nullptr;
};

// -----------------------------------------------------------------------
// MacrosPanel
// -----------------------------------------------------------------------
MacrosPanel::MacrosPanel(QWidget *parent) : QWidget(parent) {
    m_macros = loadMacros();

    QLabel *title = new QLabel("Macros");
    title->setObjectName("sectionTitle");

    m_listWidget = new QListWidget;
    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, &MacrosPanel::onRun);

    QPushButton *addBtn    = new QPushButton("Add");
    QPushButton *editBtn   = new QPushButton("Edit");
    QPushButton *removeBtn = new QPushButton("Remove");
    connect(addBtn,    &QPushButton::clicked, this, &MacrosPanel::onAdd);
    connect(editBtn,   &QPushButton::clicked, this, &MacrosPanel::onEdit);
    connect(removeBtn, &QPushButton::clicked, this, &MacrosPanel::onRemove);

    QWidget *editRow = new QWidget;
    QHBoxLayout *el = new QHBoxLayout(editRow);
    el->setContentsMargins(0,0,0,0);
    el->setSpacing(6);
    el->addWidget(addBtn);
    el->addWidget(editBtn);
    el->addWidget(removeBtn);

    QPushButton *runBtn = new QPushButton("Send to Session");
    connect(runBtn, &QPushButton::clicked, this, &MacrosPanel::onRun);

    QLabel *note = new QLabel(
        "Sends the macro's commands to the active session, or to all "
        "sessions when Multi-Exec View is on. Unless 'Execute "
        "immediately' is set, the last line is typed but not run.");
    note->setObjectName("mutedNote");
    note->setWordWrap(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8,8,8,8);
    layout->setSpacing(8);
    layout->addWidget(title);
    layout->addWidget(m_listWidget, 1);
    layout->addWidget(editRow);
    layout->addWidget(runBtn);
    layout->addWidget(note);

    populate();
}

void MacrosPanel::populate() {
    m_listWidget->clear();
    for (auto v : m_macros)
        m_listWidget->addItem(v.toObject().value("name").toString());
}

int MacrosPanel::currentIndex() const {
    int row = m_listWidget->currentRow();
    return (row >= 0 && row < m_macros.size()) ? row : -1;
}

void MacrosPanel::onAdd() {
    MacroEditDialog dlg(this);
    if (dlg.exec()) {
        m_macros.append(dlg.getMacro());
        saveMacros(m_macros);
        populate();
    }
}

void MacrosPanel::onEdit() {
    int idx = currentIndex();
    if (idx < 0) return;
    MacroEditDialog dlg(this, m_macros[idx].toObject());
    if (dlg.exec()) {
        m_macros[idx] = dlg.getMacro();
        saveMacros(m_macros);
        populate();
    }
}

void MacrosPanel::onRemove() {
    int idx = currentIndex();
    if (idx < 0) return;
    m_macros.removeAt(idx);
    saveMacros(m_macros);
    populate();
}

void MacrosPanel::onRun() {
    int idx = currentIndex();
    if (idx < 0) return;
    QJsonObject macro = m_macros[idx].toObject();
    emit runRequested(macro.value("commands").toString(),
                      macro.value("auto_execute").toBool(false));
}
