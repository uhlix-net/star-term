#pragma once
#include <QWidget>
#include <QJsonArray>

class QListWidget;

// Matches macros_panel.py exactly.
class MacrosPanel : public QWidget {
    Q_OBJECT
public:
    explicit MacrosPanel(QWidget *parent = nullptr);

signals:
    void runRequested(const QString &commands, bool autoExecute);

private slots:
    void onAdd();
    void onEdit();
    void onRemove();
    void onRun();

private:
    void populate();
    int  currentIndex() const;

    QListWidget *m_listWidget = nullptr;
    QJsonArray   m_macros;
};
