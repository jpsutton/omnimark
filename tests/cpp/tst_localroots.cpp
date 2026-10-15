#include "LocalRoots.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestLocalRoots : public QObject
{
    Q_OBJECT
private slots:
    void allowsFilesInsideRoot();
    void rejectsTraversalAndSiblings();
    void refCountsRoots();
    void rejectsSymlinkEscape();
    void containsDirectory();
    void removeAfterRename();
};

static void touch(const QString &path)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
}

void TestLocalRoots::allowsFilesInsideRoot()
{
    QTemporaryDir tmp;
    QDir(tmp.path()).mkpath(QStringLiteral("docs/img"));
    touch(tmp.filePath(QStringLiteral("docs/img/a.png")));

    LocalRoots roots;
    roots.add(tmp.filePath(QStringLiteral("docs")));
    QVERIFY(roots.isAllowed(tmp.filePath(QStringLiteral("docs/img/a.png"))));
    QVERIFY(!roots.isAllowed(tmp.filePath(QStringLiteral("docs/img/missing.png"))));
    QVERIFY(!roots.isAllowed(tmp.filePath(QStringLiteral("docs"))));
}

void TestLocalRoots::rejectsTraversalAndSiblings()
{
    QTemporaryDir tmp;
    QDir(tmp.path()).mkpath(QStringLiteral("docs"));
    QDir(tmp.path()).mkpath(QStringLiteral("docs-secret"));
    touch(tmp.filePath(QStringLiteral("secret.txt")));
    touch(tmp.filePath(QStringLiteral("docs-secret/key.txt")));

    LocalRoots roots;
    roots.add(tmp.filePath(QStringLiteral("docs")));
    QVERIFY(!roots.isAllowed(tmp.filePath(QStringLiteral("docs/../secret.txt"))));
    QVERIFY(!roots.isAllowed(tmp.filePath(QStringLiteral("docs-secret/key.txt"))));
}

void TestLocalRoots::refCountsRoots()
{
    QTemporaryDir tmp;
    touch(tmp.filePath(QStringLiteral("a.md")));

    LocalRoots roots;
    roots.add(tmp.path());
    roots.add(tmp.path());
    roots.remove(tmp.path());
    QVERIFY(roots.isAllowed(tmp.filePath(QStringLiteral("a.md"))));
    roots.remove(tmp.path());
    QVERIFY(!roots.isAllowed(tmp.filePath(QStringLiteral("a.md"))));
}

void TestLocalRoots::rejectsSymlinkEscape()
{
#ifdef Q_OS_UNIX
    QTemporaryDir tmp;
    QDir(tmp.path()).mkpath(QStringLiteral("docs"));
    touch(tmp.filePath(QStringLiteral("outside.txt")));
    QVERIFY(QFile::link(tmp.filePath(QStringLiteral("outside.txt")),
                        tmp.filePath(QStringLiteral("docs/link.txt"))));

    LocalRoots roots;
    roots.add(tmp.filePath(QStringLiteral("docs")));
    QVERIFY(!roots.isAllowed(tmp.filePath(QStringLiteral("docs/link.txt"))));
#else
    QSKIP("symlink test is Unix-only");
#endif
}

void TestLocalRoots::containsDirectory()
{
    QTemporaryDir tmp;
    QDir(tmp.path()).mkpath(QStringLiteral("docs/sub"));
    QDir(tmp.path()).mkpath(QStringLiteral("other"));

    LocalRoots roots;
    roots.add(tmp.filePath(QStringLiteral("docs")));
    QVERIFY(roots.containsDirectory(tmp.filePath(QStringLiteral("docs"))));
    QVERIFY(roots.containsDirectory(tmp.filePath(QStringLiteral("docs/sub"))));
    QVERIFY(!roots.containsDirectory(tmp.filePath(QStringLiteral("other"))));
    QVERIFY(!roots.containsDirectory(tmp.path()));
}

void TestLocalRoots::removeAfterRename()
{
    QTemporaryDir tmp;
    QDir(tmp.path()).mkpath(QStringLiteral("docs"));
    LocalRoots roots;
    const QString key = roots.add(tmp.filePath(QStringLiteral("docs")));
    QVERIFY(!key.isEmpty());
    QVERIFY(QDir(tmp.path()).rename(QStringLiteral("docs"), QStringLiteral("moved")));
    roots.remove(key);
    QVERIFY(roots.roots().isEmpty());
    // A new directory at the old path must not inherit access.
    QDir(tmp.path()).mkpath(QStringLiteral("docs"));
    touch(tmp.filePath(QStringLiteral("docs/x.md")));
    QVERIFY(!roots.isAllowed(tmp.filePath(QStringLiteral("docs/x.md"))));
}

QTEST_GUILESS_MAIN(TestLocalRoots)
#include "tst_localroots.moc"
