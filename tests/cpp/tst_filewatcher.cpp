#include "FileWatcher.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestFileWatcher : public QObject
{
    Q_OBJECT
private slots:
    void emitsOnceForBurst();
    void rearmsAfterAtomicReplace();
    void ignoresOwnSave();
    void recoversAfterDeleteAndRecreate();
};

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

static void atomicReplace(const QString &path, const QByteArray &data)
{
    const QString tmp = path + QStringLiteral(".tmp");
    writeFile(tmp, data);
    QFile::remove(path);
    QVERIFY(QFile::rename(tmp, path));
}

void TestFileWatcher::emitsOnceForBurst()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.md"));
    writeFile(path, "1");

    FileWatcher watcher;
    watcher.setPath(path);
    QSignalSpy spy(&watcher, &FileWatcher::changed);
    writeFile(path, "2");
    writeFile(path, "3");
    writeFile(path, "4");
    QVERIFY(spy.wait(2000));
    QTest::qWait(FileWatcher::DebounceMs * 3);
    QCOMPARE(spy.count(), 1);
}

void TestFileWatcher::rearmsAfterAtomicReplace()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.md"));
    writeFile(path, "1");

    FileWatcher watcher;
    watcher.setPath(path);
    QSignalSpy spy(&watcher, &FileWatcher::changed);

    atomicReplace(path, "2");
    QVERIFY(spy.wait(3000));
    atomicReplace(path, "3");
    QVERIFY(spy.wait(3000));
    QCOMPARE(spy.count(), 2);
}

void TestFileWatcher::ignoresOwnSave()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.md"));
    writeFile(path, "1");

    FileWatcher watcher;
    watcher.setPath(path);
    QSignalSpy spy(&watcher, &FileWatcher::changed);
    watcher.ignoreNextChange();
    writeFile(path, "2");
    QTest::qWait(FileWatcher::DebounceMs * 4);
    QCOMPARE(spy.count(), 0);
    writeFile(path, "3");
    QVERIFY(spy.wait(2000));
}

void TestFileWatcher::recoversAfterDeleteAndRecreate()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a.md"));
    writeFile(path, "1");

    FileWatcher watcher;
    watcher.setPath(path);
    QSignalSpy removed(&watcher, &FileWatcher::removed);
    QSignalSpy changed(&watcher, &FileWatcher::changed);
    QFile::remove(path);
    QVERIFY(removed.wait(FileWatcher::DebounceMs * (FileWatcher::MaxRearmAttempts + 5)));
    writeFile(path, "2"); // e.g. a checkout or rebuild brings it back
    QVERIFY(changed.wait(3000));
    writeFile(path, "3"); // and it is watched normally again
    QVERIFY(changed.wait(3000));
}

QTEST_GUILESS_MAIN(TestFileWatcher)
#include "tst_filewatcher.moc"
