#include "FileWatcher.h"

#include <QFileInfo>

FileWatcher::FileWatcher(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(DebounceMs);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &FileWatcher::onFileChanged);
    connect(&m_debounce, &QTimer::timeout, this, &FileWatcher::settle);
    m_poll.setInterval(1000);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        if (m_path.isEmpty() || !QFileInfo::exists(m_path))
            return;
        m_poll.stop();
        m_watcher.addPath(m_path);
        emit changed(m_path);
    });
}

void FileWatcher::setPath(const QString &path)
{
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    m_path = path;
    m_rearmAttempts = 0;
    m_ignoreNext = false;
    m_poll.stop();
    if (!m_path.isEmpty())
        m_watcher.addPath(m_path);
}

void FileWatcher::ignoreNextChange()
{
    m_ignoreNext = true;
}

void FileWatcher::onFileChanged()
{
    m_rearmAttempts = 0;
    m_debounce.start();
}

void FileWatcher::settle()
{
    if (m_path.isEmpty())
        return;

    if (!QFileInfo::exists(m_path)) {
        // Mid atomic-replace, or really deleted. Retry for a while.
        if (++m_rearmAttempts <= MaxRearmAttempts) {
            m_debounce.start();
            return;
        }
        emit removed(m_path);
        m_poll.start();
        return;
    }

    if (!m_watcher.files().contains(m_path))
        m_watcher.addPath(m_path);

    if (m_ignoreNext) {
        m_ignoreNext = false;
        return;
    }
    emit changed(m_path);
}
