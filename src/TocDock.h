#pragma once

#include <QDockWidget>
#include <QVariantList>

class QTreeWidget;
class QTreeWidgetItem;

// Outline of the current document's headings with scroll-spy highlighting.
class TocDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit TocDock(QWidget *parent = nullptr);

    // Each entry is a map with level, id, text and line (from the web page).
    void setHeadings(const QVariantList &headings);
    void setActiveHeading(const QString &id);

signals:
    void headingActivated(const QString &id, int line);

private:
    QTreeWidget *m_tree;
    QHash<QString, QTreeWidgetItem *> m_items;
    bool m_updating = false;
};
