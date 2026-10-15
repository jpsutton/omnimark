#include "LocalRoots.h"
#include "SchemeHandler.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestSchemeHandler : public QObject
{
    Q_OBJECT
private slots:
    void resolvesAppFiles();
    void blocksAppTraversal();
    void resolvesLocalOnlyInsideRoots();
    void mimeTypes();
};

static void touch(const QString &path)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
}

void TestSchemeHandler::resolvesAppFiles()
{
    QTemporaryDir tmp;
    QDir(tmp.path()).mkpath(QStringLiteral("web/assets"));
    touch(tmp.filePath(QStringLiteral("web/index.html")));
    touch(tmp.filePath(QStringLiteral("web/assets/app.js")));

    SchemeHandler handler(tmp.filePath(QStringLiteral("web")), nullptr);
    QCOMPARE(handler.resolveAppPath(QStringLiteral("/")), tmp.filePath(QStringLiteral("web/index.html")));
    QCOMPARE(handler.resolveAppPath(QStringLiteral("/assets/app.js")),
             tmp.filePath(QStringLiteral("web/assets/app.js")));
    QVERIFY(handler.resolveAppPath(QStringLiteral("/assets")).isEmpty());
    QVERIFY(handler.resolveAppPath(QStringLiteral("/missing.js")).isEmpty());
}

void TestSchemeHandler::blocksAppTraversal()
{
    QTemporaryDir tmp;
    QDir(tmp.path()).mkpath(QStringLiteral("web"));
    touch(tmp.filePath(QStringLiteral("web/index.html")));
    touch(tmp.filePath(QStringLiteral("secret.txt")));

    SchemeHandler handler(tmp.filePath(QStringLiteral("web")), nullptr);
    QVERIFY(handler.resolveAppPath(QStringLiteral("/../secret.txt")).isEmpty());
    QVERIFY(handler.resolveAppPath(QStringLiteral("/assets/../../secret.txt")).isEmpty());
}

void TestSchemeHandler::resolvesLocalOnlyInsideRoots()
{
    QTemporaryDir tmp;
    QDir(tmp.path()).mkpath(QStringLiteral("docs"));
    touch(tmp.filePath(QStringLiteral("docs/pic.png")));
    touch(tmp.filePath(QStringLiteral("other.png")));

    LocalRoots roots;
    roots.add(tmp.filePath(QStringLiteral("docs")));
    SchemeHandler handler(QString(), &roots);

    const QString inside = QDir(tmp.filePath(QStringLiteral("docs/pic.png"))).canonicalPath();
    QCOMPARE(handler.resolveLocalPath(tmp.filePath(QStringLiteral("docs/pic.png"))),
             QFileInfo(inside).canonicalFilePath());
    QVERIFY(handler.resolveLocalPath(tmp.filePath(QStringLiteral("other.png"))).isEmpty());
    QVERIFY(handler.resolveLocalPath(tmp.filePath(QStringLiteral("docs/../other.png"))).isEmpty());
}

void TestSchemeHandler::mimeTypes()
{
    QCOMPARE(SchemeHandler::mimeTypeFor(QStringLiteral("a.js")), QByteArray("text/javascript"));
    QCOMPARE(SchemeHandler::mimeTypeFor(QStringLiteral("a.wasm")), QByteArray("application/wasm"));
    QCOMPARE(SchemeHandler::mimeTypeFor(QStringLiteral("a.svg")), QByteArray("image/svg+xml"));
    QCOMPARE(SchemeHandler::mimeTypeFor(QStringLiteral("a.png")), QByteArray("image/png"));
}

QTEST_GUILESS_MAIN(TestSchemeHandler)
#include "tst_schemehandler.moc"
