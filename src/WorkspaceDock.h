#pragma once

#include <QDockWidget>

class QFileSystemModel;
class QLabel;
class QLineEdit;
class QSortFilterProxyModel;
class QTreeView;

// Folder tree limited to Markdown files. Opening a folder also grants the web
// views access to files beneath it (images, linked documents).
class WorkspaceDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit WorkspaceDock(QWidget *parent = nullptr);

    void setRoot(const QString &path);
    QString root() const { return m_root; }

    // Selects `path` in the tree if it lies inside the workspace.
    void reveal(const QString &path);

    static const QStringList &nameFilters();

signals:
    void fileActivated(const QString &path, bool newTab);
    void rootChanged(const QString &root);

private:
    QFileSystemModel *m_model;
    QSortFilterProxyModel *m_proxy;
    QTreeView *m_tree;
    QLineEdit *m_filter;
    QLabel *m_rootLabel;
    QString m_root;
};
