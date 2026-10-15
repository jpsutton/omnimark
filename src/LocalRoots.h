#pragma once

#include <QHash>
#include <QString>

// Allow-list of directories whose files may be served to web content via
// omnimark://local/<absolute path>. Roots are reference counted so that two
// tabs sharing a directory don't revoke each other's access on close.
class LocalRoots
{
public:
    // Returns the key to pass to remove(); empty if `dir` doesn't exist.
    // Keys are canonical paths captured now, so removal still works after
    // the directory is renamed or deleted.
    QString add(const QString &dir);
    void remove(const QString &key);
    void clear();

    // True when `path` resolves (symlinks included) to a file inside a root.
    bool isAllowed(const QString &path) const;

    // True when existing directory `dir` is a root or lies inside one.
    bool containsDirectory(const QString &dir) const;

    QStringList roots() const { return m_roots.keys(); }

private:
    QHash<QString, int> m_roots;
};
