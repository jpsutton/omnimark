#pragma once

#include <QDockWidget>
#include <QFutureWatcher>

class QLabel;
class QLineEdit;
class QCheckBox;
class QTreeWidget;

struct SearchHit
{
    QString path;
    int line = 0; // 1-based
    QString text;
};

// Find in Folder: scans Markdown files under the workspace in the background.
class SearchDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit SearchDock(QWidget *parent = nullptr);
    ~SearchDock() override;

    void setRoot(const QString &root);
    void activate(const QString &initial = {});

    // Exposed for tests.
    static QList<SearchHit> search(const QString &root, const QString &needle, bool caseSensitive,
                                   int maxHits = 2000);

signals:
    void hitActivated(const QString &path, int line);

private:
    void start();
    void showResults();

    QString m_root;
    QLineEdit *m_input;
    QCheckBox *m_case;
    QLabel *m_status;
    QTreeWidget *m_results;
    QFutureWatcher<QList<SearchHit>> m_watcher;
};
