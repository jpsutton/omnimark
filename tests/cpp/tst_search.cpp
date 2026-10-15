#include "SearchDock.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestSearch : public QObject
{
    Q_OBJECT
private slots:
    void findsMatchesInMarkdownOnly();
    void respectsCaseAndLimit();
};

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

void TestSearch::findsMatchesInMarkdownOnly()
{
    QTemporaryDir dir;
    QDir(dir.path()).mkpath(QStringLiteral("a/b"));
    writeFile(dir.filePath(QStringLiteral("one.md")), "alpha\nbeta needle\ngamma\n");
    writeFile(dir.filePath(QStringLiteral("a/b/two.markdown")), "Needle up top\n");
    writeFile(dir.filePath(QStringLiteral("a/skip.txt")), "needle\n");

    const QList<SearchHit> hits = SearchDock::search(dir.path(), QStringLiteral("needle"), false);
    QCOMPARE(hits.size(), 2);
    bool sawOne = false;
    for (const SearchHit &hit : hits) {
        if (hit.path.endsWith(QStringLiteral("one.md"))) {
            QCOMPARE(hit.line, 2);
            QCOMPARE(hit.text, QStringLiteral("beta needle"));
            sawOne = true;
        }
    }
    QVERIFY(sawOne);
}

void TestSearch::respectsCaseAndLimit()
{
    QTemporaryDir dir;
    writeFile(dir.filePath(QStringLiteral("x.md")), "Needle\nneedle\nneedle\nneedle\n");
    QCOMPARE(SearchDock::search(dir.path(), QStringLiteral("Needle"), true).size(), 1);
    QCOMPARE(SearchDock::search(dir.path(), QStringLiteral("needle"), false, 2).size(), 2);
    QVERIFY(SearchDock::search(dir.path(), QString(), false).isEmpty());
}

QTEST_GUILESS_MAIN(TestSearch)
#include "tst_search.moc"
