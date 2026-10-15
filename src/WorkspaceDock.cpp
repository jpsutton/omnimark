#include "WorkspaceDock.h"

#include <QApplication>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QVBoxLayout>

namespace {

// Hides folders that contain no Markdown at any depth would be expensive to
// compute up front, so folders are always shown; the text filter matches files.
class FilterProxy : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        const QModelIndex index = sourceModel()->index(row, 0, parent);
        auto *fs = static_cast<QFileSystemModel *>(sourceModel());
        if (fs->isDir(index))
            return true;
        return QSortFilterProxyModel::filterAcceptsRow(row, parent);
    }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        auto *fs = static_cast<QFileSystemModel *>(sourceModel());
        const bool leftDir = fs->isDir(left);
        const bool rightDir = fs->isDir(right);
        if (leftDir != rightDir)
            return leftDir;
        return QString::localeAwareCompare(fs->fileName(left), fs->fileName(right)) < 0;
    }
};

}

const QStringList &WorkspaceDock::nameFilters()
{
    static const QStringList filters = {
        QStringLiteral("*.md"), QStringLiteral("*.markdown"), QStringLiteral("*.mdown"),
        QStringLiteral("*.mkd"), QStringLiteral("*.mdx"),
    };
    return filters;
}

WorkspaceDock::WorkspaceDock(QWidget *parent)
    : QDockWidget(tr("Files"), parent)
    , m_model(new QFileSystemModel(this))
    , m_proxy(new FilterProxy(this))
    , m_tree(new QTreeView)
    , m_filter(new QLineEdit)
    , m_rootLabel(new QLabel)
{
    setObjectName(QStringLiteral("WorkspaceDock"));

    m_model->setNameFilters(nameFilters());
    m_model->setNameFilterDisables(false);
    m_model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setRecursiveFilteringEnabled(false);
    m_proxy->setDynamicSortFilter(true);

    m_tree->setModel(m_proxy);
    m_tree->setHeaderHidden(true);
    for (int column = 1; column < m_model->columnCount(); ++column)
        m_tree->hideColumn(column);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(0, Qt::AscendingOrder);
    m_tree->setUniformRowHeights(true);
    m_tree->setAnimated(false);

    m_filter->setPlaceholderText(tr("Filter files…"));
    m_filter->setClearButtonEnabled(true);
    connect(m_filter, &QLineEdit::textChanged, m_proxy, &QSortFilterProxyModel::setFilterWildcard);

    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    QFont bold = m_rootLabel->font();
    bold.setBold(true);
    m_rootLabel->setFont(bold);
    m_rootLabel->setContentsMargins(4, 2, 4, 2);
    m_rootLabel->hide();
    layout->addWidget(m_rootLabel);
    layout->addWidget(m_filter);
    layout->addWidget(m_tree);
    setWidget(container);

    auto activate = [this](const QModelIndex &proxyIndex, bool newTab) {
        const QModelIndex index = m_proxy->mapToSource(proxyIndex);
        if (!m_model->isDir(index))
            emit fileActivated(m_model->filePath(index), newTab);
    };
    connect(m_tree, &QTreeView::activated, this, [activate](const QModelIndex &index) {
        activate(index, QApplication::keyboardModifiers() & Qt::ControlModifier);
    });
    connect(m_tree, &QTreeView::clicked, this, [activate](const QModelIndex &index) {
        // Single click previews in the current tab; Ctrl+click opens a new tab.
        activate(index, QApplication::keyboardModifiers() & Qt::ControlModifier);
    });
}

void WorkspaceDock::setRoot(const QString &path)
{
    m_root = path;
    const QModelIndex sourceRoot = m_model->setRootPath(path);
    m_tree->setRootIndex(m_proxy->mapFromSource(sourceRoot));
    // The dock title doubles as the toolbar toggle's label, so it stays "Files".
    m_rootLabel->setText(QDir(path).dirName());
    m_rootLabel->setToolTip(QDir::toNativeSeparators(path));
    m_rootLabel->setVisible(!path.isEmpty());
    emit rootChanged(path);
}

void WorkspaceDock::reveal(const QString &path)
{
    if (m_root.isEmpty() || !path.startsWith(m_root))
        return;
    const QModelIndex index = m_proxy->mapFromSource(m_model->index(path));
    if (!index.isValid())
        return;
    m_tree->setCurrentIndex(index);
    m_tree->scrollTo(index);
}
