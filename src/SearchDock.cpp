#include "SearchDock.h"

#include "WorkspaceDock.h"

#include <QCheckBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTextStream>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

static constexpr int PathRole = Qt::UserRole;
static constexpr int LineRole = Qt::UserRole + 1;

SearchDock::SearchDock(QWidget *parent)
    : QDockWidget(tr("Search"), parent)
    , m_input(new QLineEdit)
    , m_case(new QCheckBox(tr("Match case")))
    , m_status(new QLabel)
    , m_results(new QTreeWidget)
{
    setObjectName(QStringLiteral("SearchDock"));
    m_input->setPlaceholderText(tr("Find in folder…"));
    m_input->setClearButtonEnabled(true);
    m_results->setHeaderHidden(true);
    m_results->setUniformRowHeights(true);
    m_results->setRootIsDecorated(true);
    m_status->setWordWrap(true);

    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(m_input);
    layout->addWidget(m_case);
    layout->addWidget(m_status);
    layout->addWidget(m_results, 1);
    setWidget(container);

    connect(m_input, &QLineEdit::returnPressed, this, &SearchDock::start);
    connect(m_case, &QCheckBox::toggled, this, [this] {
        if (!m_input->text().isEmpty())
            start();
    });
    connect(&m_watcher, &QFutureWatcher<QList<SearchHit>>::finished, this, &SearchDock::showResults);
    auto activate = [this](QTreeWidgetItem *item) {
        const QString path = item->data(0, PathRole).toString();
        if (!path.isEmpty())
            emit hitActivated(path, item->data(0, LineRole).toInt());
    };
    connect(m_results, &QTreeWidget::itemClicked, this, activate);
    connect(m_results, &QTreeWidget::itemActivated, this, activate);
}

SearchDock::~SearchDock()
{
    m_watcher.waitForFinished();
}

void SearchDock::setRoot(const QString &root)
{
    m_root = root;
    m_results->clear();
    m_status->setText(root.isEmpty() ? tr("Open a folder to search it.") : QString());
}

void SearchDock::activate(const QString &initial)
{
    show();
    raise();
    if (!initial.isEmpty())
        m_input->setText(initial);
    m_input->setFocus();
    m_input->selectAll();
}

QList<SearchHit> SearchDock::search(const QString &root, const QString &needle, bool caseSensitive, int maxHits)
{
    QList<SearchHit> hits;
    if (root.isEmpty() || needle.isEmpty())
        return hits;
    const Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    QDirIterator it(root, WorkspaceDock::nameFilters(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext() && hits.size() < maxHits) {
        const QString path = it.next();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QTextStream stream(&file);
        int line = 0;
        while (!stream.atEnd() && hits.size() < maxHits) {
            const QString text = stream.readLine();
            ++line;
            if (text.contains(needle, cs))
                hits.append({ path, line, text.trimmed() });
        }
    }
    return hits;
}

void SearchDock::start()
{
    const QString needle = m_input->text();
    if (m_root.isEmpty()) {
        m_status->setText(tr("Open a folder to search it."));
        return;
    }
    if (needle.isEmpty())
        return;
    m_watcher.waitForFinished();
    m_status->setText(tr("Searching…"));
    m_results->clear();
    m_watcher.setFuture(QtConcurrent::run(&SearchDock::search, m_root, needle, m_case->isChecked(), 2000));
}

void SearchDock::showResults()
{
    const QList<SearchHit> hits = m_watcher.result();
    m_results->clear();
    QHash<QString, QTreeWidgetItem *> files;
    const QDir root(m_root);
    for (const SearchHit &hit : hits) {
        QTreeWidgetItem *&fileItem = files[hit.path];
        if (!fileItem) {
            fileItem = new QTreeWidgetItem(m_results);
            fileItem->setText(0, root.relativeFilePath(hit.path));
            fileItem->setToolTip(0, hit.path);
            fileItem->setData(0, PathRole, hit.path);
            fileItem->setData(0, LineRole, 1);
        }
        auto *item = new QTreeWidgetItem(fileItem);
        item->setText(0, QStringLiteral("%1: %2").arg(hit.line).arg(hit.text.left(200)));
        item->setToolTip(0, hit.text);
        item->setData(0, PathRole, hit.path);
        item->setData(0, LineRole, hit.line);
    }
    m_results->expandAll();
    const QString matches = hits.size() == 1 ? tr("1 match") : tr("%1 matches").arg(hits.size());
    const QString inFiles = files.size() == 1 ? tr("in 1 file") : tr("in %1 files").arg(files.size());
    m_status->setText(hits.isEmpty() ? tr("No matches.") : matches + u' ' + inFiles);
}
