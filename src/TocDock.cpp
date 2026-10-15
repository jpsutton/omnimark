#include "TocDock.h"

#include <QHeaderView>
#include <QTreeWidget>

static constexpr int IdRole = Qt::UserRole;
static constexpr int LineRole = Qt::UserRole + 1;

TocDock::TocDock(QWidget *parent)
    : QDockWidget(tr("Outline"), parent)
    , m_tree(new QTreeWidget)
{
    setObjectName(QStringLiteral("TocDock"));
    m_tree->setHeaderHidden(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setExpandsOnDoubleClick(false);
    m_tree->setIndentation(14);
    setWidget(m_tree);

    auto activate = [this](QTreeWidgetItem *item) {
        if (item && !m_updating)
            emit headingActivated(item->data(0, IdRole).toString(), item->data(0, LineRole).toInt());
    };
    connect(m_tree, &QTreeWidget::itemClicked, this, activate);
    connect(m_tree, &QTreeWidget::itemActivated, this, activate);
}

void TocDock::setHeadings(const QVariantList &headings)
{
    m_updating = true;
    const QString current = m_tree->currentItem() ? m_tree->currentItem()->data(0, IdRole).toString() : QString();
    m_tree->clear();
    m_items.clear();

    // Stack of (level, item) to nest headings under the nearest shallower one.
    QList<QPair<int, QTreeWidgetItem *>> stack;
    for (const QVariant &value : headings) {
        const QVariantMap heading = value.toMap();
        const int level = heading.value(QStringLiteral("level")).toInt();
        while (!stack.isEmpty() && stack.last().first >= level)
            stack.removeLast();
        auto *item = stack.isEmpty() ? new QTreeWidgetItem(m_tree) : new QTreeWidgetItem(stack.last().second);
        const QString text = heading.value(QStringLiteral("text")).toString();
        item->setText(0, text);
        item->setToolTip(0, text);
        const QString id = heading.value(QStringLiteral("id")).toString();
        item->setData(0, IdRole, id);
        item->setData(0, LineRole, heading.value(QStringLiteral("line")).toInt());
        m_items.insert(id, item);
        stack.append({ level, item });
    }
    m_tree->expandAll();
    if (QTreeWidgetItem *item = m_items.value(current))
        m_tree->setCurrentItem(item);
    m_updating = false;
}

void TocDock::setActiveHeading(const QString &id)
{
    QTreeWidgetItem *item = m_items.value(id);
    if (!item)
        return;
    m_updating = true;
    m_tree->setCurrentItem(item);
    m_tree->scrollToItem(item);
    m_updating = false;
}
