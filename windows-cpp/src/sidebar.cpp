#include "sidebar.h"
#include "config.h"
#include "icons.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDropEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

static const QString DEFAULT_FOLDER = "General";
static const int     FOLDER_ROLE    = Qt::UserRole + 1;

// -----------------------------------------------------------------------
// SessionEditDialog (inner)
// -----------------------------------------------------------------------
class SessionEditDialog : public QDialog {
public:
    SessionEditDialog(QWidget *parent, const QJsonObject &session, const QStringList &folders)
        : QDialog(parent)
    {
        setWindowTitle("Session");

        QString type = session.value("type").toString("ssh");

        m_nameEdit   = new QLineEdit(session.value("name").toString());
        m_folderEdit = new QComboBox;
        m_folderEdit->setEditable(true);
        m_folderEdit->addItems(folders);
        m_folderEdit->setCurrentText(session.value("folder").toString());
        m_folderEdit->lineEdit()->setPlaceholderText(DEFAULT_FOLDER);

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

        QFormLayout *sshForm = new QFormLayout(m_sshGroup);
        sshForm->addRow(m_useKeyCheck);
        sshForm->addRow("SSH key override:", keyRow);

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
        root->addWidget(buttons);

        connect(m_typeCombo, &QComboBox::currentTextChanged,
                this, &SessionEditDialog::onTypeChanged);
        onTypeChanged(m_typeCombo->currentText());
    }

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
    QLineEdit  *m_nameEdit, *m_hostEdit, *m_portEdit, *m_usernameEdit, *m_keyPathEdit;
    QComboBox  *m_folderEdit, *m_typeCombo;
    QCheckBox  *m_useKeyCheck;
    QGroupBox  *m_sshGroup;
};

// -----------------------------------------------------------------------
// SessionTreeWidget
// -----------------------------------------------------------------------
SessionTreeWidget::SessionTreeWidget(QWidget *parent) : QTreeWidget(parent) {
    setDragDropMode(QAbstractItemView::InternalMove);
    setDragEnabled(true);
}

void SessionTreeWidget::dropEvent(QDropEvent *event) {
    QTreeWidgetItem *source = currentItem();
    if (!source || source->data(0, Qt::UserRole).isNull()) {
        event->ignore(); return;
    }

    QTreeWidgetItem *target = itemAt(event->position().toPoint());
    if (!target) { event->ignore(); return; }

    QString targetFolder;
    if (!target->data(0, Qt::UserRole).isNull()) {
        // dropped on a session — use its folder
        QTreeWidgetItem *parent = target->parent();
        if (parent) targetFolder = parent->data(0, FOLDER_ROLE).toString();
    } else {
        targetFolder = target->data(0, FOLDER_ROLE).toString();
    }

    if (targetFolder.isEmpty()) { event->ignore(); return; }

    event->accept();
    emit sessionDropped(source->data(0, Qt::UserRole).toInt(), targetFolder);
}

// -----------------------------------------------------------------------
// SessionSidebar
// -----------------------------------------------------------------------
SessionSidebar::SessionSidebar(QWidget *parent) : QWidget(parent) {
    m_sessions = loadSessions();
    m_folders  = loadFolders();

    m_listWidget = new SessionTreeWidget;
    m_listWidget->setHeaderHidden(true);
    m_listWidget->setIndentation(12);
    connect(m_listWidget, &QTreeWidget::itemDoubleClicked,
            this, [this]() { onConnect(); });
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listWidget, &QTreeWidget::customContextMenuRequested,
            this, &SessionSidebar::onContextMenu);
    connect(m_listWidget, &SessionTreeWidget::sessionDropped,
            this, &SessionSidebar::onSessionDropped);

    QPushButton *addBtn    = new QPushButton("Add");
    QPushButton *editBtn   = new QPushButton("Edit");
    QPushButton *removeBtn = new QPushButton("Remove");
    connect(addBtn,    &QPushButton::clicked, this, [this]() { onAdd(); });
    connect(editBtn,   &QPushButton::clicked, this, &SessionSidebar::onEdit);
    connect(removeBtn, &QPushButton::clicked, this, &SessionSidebar::onRemove);

    QWidget *editRow = new QWidget;
    QHBoxLayout *el = new QHBoxLayout(editRow);
    el->setContentsMargins(0,0,0,0);
    el->setSpacing(6);
    el->addWidget(addBtn);
    el->addWidget(editBtn);
    el->addWidget(removeBtn);

    QPushButton *connectBtn = new QPushButton("Connect");
    connect(connectBtn, &QPushButton::clicked, this, &SessionSidebar::onConnect);

    QLabel *title = new QLabel("Sessions");
    title->setObjectName("sectionTitle");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8,8,8,8);
    layout->setSpacing(8);
    layout->addWidget(title);
    layout->addWidget(m_listWidget, 1);
    layout->addWidget(editRow);
    layout->addWidget(connectBtn);

    populate();
}

QStringList SessionSidebar::allFolderNames() const {
    QSet<QString> names;
    for (const auto &v : m_folders) names.insert(v.toString());
    for (const auto &v : m_sessions) names.insert(v.toObject().value("folder").toString(DEFAULT_FOLDER));
    names.insert(DEFAULT_FOLDER);
    QStringList list = names.values();
    std::sort(list.begin(), list.end(), [](const QString &a, const QString &b) {
        return a.toLower() < b.toLower();
    });
    return list;
}

int SessionSidebar::folderSessionCount(const QString &folderName) const {
    int count = 0;
    for (const auto &v : m_sessions) {
        QString f = v.toObject().value("folder").toString();
        if (f.isEmpty()) f = DEFAULT_FOLDER;
        if (f == folderName) ++count;
    }
    return count;
}

void SessionSidebar::populate() {
    m_listWidget->clear();

    // Build folder->session-indices map
    QMap<QString, QList<int>> folderMap;
    for (int i = 0; i < m_sessions.size(); ++i) {
        QString folder = m_sessions[i].toObject().value("folder").toString();
        if (folder.isEmpty()) folder = DEFAULT_FOLDER;
        folderMap[folder].append(i);
    }

    // All folders: from sessions + explicit folders + DEFAULT
    QSet<QString> allFolders;
    for (const QString &f : folderMap.keys()) allFolders.insert(f);
    for (const auto &v : m_folders) allFolders.insert(v.toString());
    allFolders.insert(DEFAULT_FOLDER);

    QStringList sortedFolders = allFolders.values();
    std::sort(sortedFolders.begin(), sortedFolders.end(), [](const QString &a, const QString &b) {
        return a.toLower() < b.toLower();
    });

    for (const QString &folder : sortedFolders) {
        QTreeWidgetItem *header = new QTreeWidgetItem({folder});
        header->setFlags(Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
        QFont f = header->font(0);
        f.setBold(true);
        header->setFont(0, f);
        header->setIcon(0, Icons::folderIcon());
        header->setData(0, FOLDER_ROLE, folder);
        m_listWidget->addTopLevelItem(header);

        QList<int> indices = folderMap.value(folder);
        std::sort(indices.begin(), indices.end(), [this](int a, int b) {
            return m_sessions[a].toObject().value("name").toString().toLower() <
                   m_sessions[b].toObject().value("name").toString().toLower();
        });

        for (int idx : indices) {
            QJsonObject sess = m_sessions[idx].toObject();
            QTreeWidgetItem *item = new QTreeWidgetItem({sess.value("name").toString()});
            item->setData(0, Qt::UserRole, idx);
            QString sessType = sess.value("type").toString("ssh");
            item->setIcon(0, (sessType == "rdp") ? Icons::rdpIcon(16) : Icons::sshIcon(16));
            header->addChild(item);
        }
    }
    m_listWidget->expandAll();
}

int SessionSidebar::currentIndex() const {
    QTreeWidgetItem *item = m_listWidget->currentItem();
    if (!item) return -1;
    QVariant v = item->data(0, Qt::UserRole);
    return v.isNull() ? -1 : v.toInt();
}

void SessionSidebar::onAdd(const QString &folder) {
    SessionEditDialog dlg(this, {}, allFolderNames());
    if (!folder.isEmpty()) dlg.findChild<QComboBox*>()->setCurrentText(folder);
    if (dlg.exec()) {
        m_sessions.append(dlg.getSession());
        saveSessions(m_sessions);
        populate();
    }
}

void SessionSidebar::onEdit() {
    int idx = currentIndex();
    if (idx < 0) return;
    SessionEditDialog dlg(this, m_sessions[idx].toObject(), allFolderNames());
    if (dlg.exec()) {
        m_sessions[idx] = dlg.getSession();
        saveSessions(m_sessions);
        populate();
    }
}

void SessionSidebar::onRemove() {
    int idx = currentIndex();
    if (idx < 0) return;
    m_sessions.removeAt(idx);
    saveSessions(m_sessions);
    populate();
}

void SessionSidebar::onConnect() {
    int idx = currentIndex();
    if (idx < 0) return;
    emit connectRequested(m_sessions[idx].toObject());
}

void SessionSidebar::onContextMenu(const QPoint &pos) {
    QTreeWidgetItem *item = m_listWidget->itemAt(pos);
    int idx = -1;
    if (item) idx = item->data(0, Qt::UserRole).isNull() ? -1 : item->data(0, Qt::UserRole).toInt();

    if (idx >= 0) {
        m_listWidget->setCurrentItem(item);
        QString folder = m_sessions[idx].toObject().value("folder").toString();
        if (folder.isEmpty()) folder = DEFAULT_FOLDER;

        QMenu menu(this);
        QAction *newSess = menu.addAction("New Session...");
        QAction *edit    = menu.addAction("Edit");
        QAction *remove  = menu.addAction("Remove");
        QAction *chosen  = menu.exec(m_listWidget->mapToGlobal(pos));
        if (chosen == newSess) onAdd(folder);
        else if (chosen == edit)   onEdit();
        else if (chosen == remove) onRemove();
        return;
    }

    QString folderName = item ? item->data(0, FOLDER_ROLE).toString() : QString();

    QMenu menu(this);
    QAction *newSess        = menu.addAction("New Session...");
    QAction *newFolder      = menu.addAction("New Folder...");
    QAction *renameFolder   = !folderName.isEmpty() ? menu.addAction("Rename Folder") : nullptr;
    QAction *removeFolder   = nullptr;
    if (!folderName.isEmpty()) {
        bool explicit_ = false;
        for (const auto &v : m_folders)
            if (v.toString() == folderName) { explicit_ = true; break; }
        if (explicit_ && folderSessionCount(folderName) == 0)
            removeFolder = menu.addAction("Remove Folder");
    }

    QAction *chosen = menu.exec(m_listWidget->mapToGlobal(pos));
    if (chosen == newSess)            onAdd(folderName);
    else if (chosen == newFolder)     onNewFolder();
    else if (renameFolder && chosen == renameFolder) onRenameFolder(folderName);
    else if (removeFolder && chosen == removeFolder) onRemoveFolder(folderName);
}

void SessionSidebar::onNewFolder() {
    bool ok = false;
    QString name = QInputDialog::getText(this, "New Folder", "Folder name:", QLineEdit::Normal, "", &ok);
    name = name.trimmed();
    if (!ok || name.isEmpty()) return;
    bool exists = false;
    for (const auto &v : m_folders) if (v.toString() == name) { exists = true; break; }
    if (!exists) {
        m_folders.append(name);
        saveFolders(m_folders);
    }
    populate();
}

void SessionSidebar::onRenameFolder(const QString &folderName) {
    bool ok = false;
    QString newName = QInputDialog::getText(
        this, "Rename Folder", "Folder name:", QLineEdit::Normal, folderName, &ok);
    newName = newName.trimmed();
    if (!ok || newName.isEmpty() || newName == folderName) return;

    // Update sessions
    for (int i = 0; i < m_sessions.size(); ++i) {
        QJsonObject s = m_sessions[i].toObject();
        QString f = s.value("folder").toString();
        if (f.isEmpty()) f = DEFAULT_FOLDER;
        if (f == folderName) {
            s["folder"] = (newName == DEFAULT_FOLDER) ? "" : newName;
            m_sessions[i] = s;
        }
    }
    saveSessions(m_sessions);

    // Update folders list
    for (int i = 0; i < m_folders.size(); ++i)
        if (m_folders[i].toString() == folderName) { m_folders.removeAt(i); break; }
    if (newName != DEFAULT_FOLDER) {
        bool exists = false;
        for (const auto &v : m_folders) if (v.toString() == newName) { exists = true; break; }
        if (!exists) m_folders.append(newName);
    }
    saveFolders(m_folders);
    populate();
}

void SessionSidebar::onRemoveFolder(const QString &folderName) {
    for (int i = 0; i < m_folders.size(); ++i)
        if (m_folders[i].toString() == folderName) { m_folders.removeAt(i); break; }
    saveFolders(m_folders);
    populate();
}

void SessionSidebar::onSessionDropped(int sessionIdx, const QString &targetFolder) {
    QJsonObject s = m_sessions[sessionIdx].toObject();
    QString curFolder = s.value("folder").toString();
    if (curFolder.isEmpty()) curFolder = DEFAULT_FOLDER;
    if (targetFolder == curFolder) return;
    s["folder"] = (targetFolder == DEFAULT_FOLDER) ? "" : targetFolder;
    m_sessions[sessionIdx] = s;
    saveSessions(m_sessions);
    populate();
}

void SessionSidebar::reload() {
    m_sessions = loadSessions();
    m_folders  = loadFolders();
    populate();
}
