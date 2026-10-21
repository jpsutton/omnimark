#include "Updater.h"

#include <QTest>

class TestUpdater : public QObject
{
    Q_OBJECT
private slots:
    void parseManifest_data();
    void parseManifest();
    void unsupportedWithoutBuildCommit();
};

static const QString Own = QStringLiteral("1111111111111111111111111111111111111111");
static const QString Other = QStringLiteral("2222222222222222222222222222222222222222");

void TestUpdater::parseManifest_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<QString>("version"); // empty: no update

    QTest::newRow("newer") << QByteArray(R"({"version":"2026.10.22","commit":")" + Other.toLatin1() + R"("})")
                           << QStringLiteral("2026.10.22");
    QTest::newRow("same commit") << QByteArray(R"({"version":"2026.10.22","commit":")" + Own.toLatin1() + R"("})")
                                 << QString();
    QTest::newRow("short commit") << QByteArray(R"({"version":"2026.10.22","commit":"2222"})") << QString();
    QTest::newRow("no version") << QByteArray(R"({"commit":")" + Other.toLatin1() + R"("})") << QString();
    QTest::newRow("not json") << QByteArray("<html>Not Found</html>") << QString();
    QTest::newRow("empty") << QByteArray() << QString();
}

void TestUpdater::parseManifest()
{
    QFETCH(QByteArray, json);
    QFETCH(QString, version);
    const auto release = Updater::parseManifest(json, Own);
    QCOMPARE(release.has_value(), !version.isEmpty());
    if (release) {
        QCOMPARE(release->version, version);
        QCOMPARE(release->commit, Other);
    }
}

void TestUpdater::unsupportedWithoutBuildCommit()
{
    // Dev builds and non-macOS builds never update themselves.
    if (QStringLiteral(OMNIMARK_BUILD_COMMIT).isEmpty())
        QVERIFY(!Updater::isSupported());
#ifndef Q_OS_MACOS
    QVERIFY(!Updater::isSupported());
#endif
}

QTEST_MAIN(TestUpdater)
#include "tst_updater.moc"
