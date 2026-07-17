#pragma once
#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>
#include <QTreeWidget>

class QTreeWidget;
class QTreeWidgetItem;

// Matches sidebar.py exactly.

class SessionTreeWidget : public QTreeWidget {
    Q_OBJECT
public:
    explicit SessionTreeWidget(QWidget *parent = nullptr);

signals:
    void sessionDropped(int sessionIdx, const QString &targetFolder);

protected:
    void dropEvent(QDropEvent *event) override;
};

class SessionSidebar : public QWidget {
    Q_OBJECT
public:
    explicit SessionSidebar(QWidget *parent = nullptr);
    void reload();

signals:
    void connectRequested(const QJsonObject &session);

private slots:
    void onAdd(const QString &folder = QString());
    void onEdit();
    void onRemove();
    void onConnect();
    void onContextMenu(const QPoint &pos);
    void onNewFolder();
    void onRenameFolder(const QString &folderName);
    void onRemoveFolder(const QString &folderName);
    void onSessionDropped(int sessionIdx, const QString &targetFolder);

private:
    void    populate();
    int     currentIndex() const;
    QStringList allFolderNames() const;
    int     folderSessionCount(const QString &folderName) const;

    SessionTreeWidget *m_listWidget = nullptr;
    QJsonArray         m_sessions;
    QJsonArray         m_folders;
};
