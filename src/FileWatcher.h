#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

// Watches one file and emits changed() once per burst of modifications.
// Editors often save by writing a temp file and renaming it over the
// original; QFileSystemWatcher then drops the path, so we re-arm it.
class FileWatcher : public QObject
{
    Q_OBJECT
public:
    explicit FileWatcher(QObject *parent = nullptr);

    void setPath(const QString &path);
    QString path() const { return m_path; }

    // Suppress the next change notification (used right after our own save).
    void ignoreNextChange();

    static constexpr int DebounceMs = 120;
    static constexpr int MaxRearmAttempts = 20;

signals:
    void changed(const QString &path);
    void removed(const QString &path);

private:
    void onFileChanged();
    void settle();

    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QTimer m_poll; // after a delete, waits for the file to come back
    QString m_path;
    int m_rearmAttempts = 0;
    bool m_ignoreNext = false;
};
