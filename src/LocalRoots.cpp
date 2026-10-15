#include "LocalRoots.h"

#include <QDir>
#include <QFileInfo>

// Windows and (by default) macOS file systems are case-insensitive, so a
// differently cased path still points inside the same root.
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
static constexpr Qt::CaseSensitivity PathCase = Qt::CaseInsensitive;
#else
static constexpr Qt::CaseSensitivity PathCase = Qt::CaseSensitive;
#endif

static bool isInside(const QString &path, const QString &root)
{
    if (path.compare(root, PathCase) == 0)
        return true;
    const QString prefix = root.endsWith(u'/') ? root : root + u'/';
    return path.startsWith(prefix, PathCase);
}

static QString canonicalDir(const QString &dir)
{
    const QString canonical = QFileInfo(dir).canonicalFilePath();
    return canonical.isEmpty() ? QString() : QDir::cleanPath(canonical);
}

QString LocalRoots::add(const QString &dir)
{
    const QString key = canonicalDir(dir);
    if (!key.isEmpty())
        ++m_roots[key];
    return key;
}

void LocalRoots::remove(const QString &key)
{
    auto it = m_roots.find(key);
    if (it == m_roots.end())
        it = m_roots.find(canonicalDir(key));
    if (it == m_roots.end())
        return;
    if (--it.value() <= 0)
        m_roots.erase(it);
}

void LocalRoots::clear()
{
    m_roots.clear();
}

bool LocalRoots::isAllowed(const QString &path) const
{
    const QString file = QFileInfo(path).canonicalFilePath();
    if (file.isEmpty())
        return false;
    if (QFileInfo(file).isDir())
        return false; // directories are never served
    for (auto it = m_roots.cbegin(); it != m_roots.cend(); ++it) {
        if (isInside(file, it.key()))
            return true;
    }
    return false;
}

bool LocalRoots::containsDirectory(const QString &dir) const
{
    const QString canonical = canonicalDir(dir);
    if (canonical.isEmpty())
        return false;
    for (auto it = m_roots.cbegin(); it != m_roots.cend(); ++it) {
        if (isInside(canonical, it.key()))
            return true;
    }
    return false;
}
