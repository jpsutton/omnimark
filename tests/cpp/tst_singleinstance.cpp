#include "SingleInstance.h"

#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QUuid>

class TestSingleInstance : public QObject
{
    Q_OBJECT
private slots:
    void roundTripsEncoding();
    void forwardsToPrimary();
    void noPrimaryMeansNoForward();
};

void TestSingleInstance::roundTripsEncoding()
{
    const QStringList files = { QStringLiteral("/a b/c.md"), QStringLiteral("/ünï/çødé.md") };
    QCOMPARE(SingleInstance::decode(SingleInstance::encode(files)), files);
    QVERIFY(SingleInstance::decode("garbage").isEmpty());
}

void TestSingleInstance::forwardsToPrimary()
{
    const QString key = QStringLiteral("omnimark-test-") + QUuid::createUuid().toString(QUuid::Id128);
    SingleInstance primary(key);
    QVERIFY(primary.listen());
    QSignalSpy spy(&primary, &SingleInstance::filesReceived);

    // The secondary blocks while writing, so run it from a nested event loop turn.
    bool forwarded = false;
    QTimer::singleShot(0, this, [&] {
        SingleInstance secondary(key);
        forwarded = secondary.forwardToPrimary({ QStringLiteral("/tmp/x.md") });
    });
    QVERIFY(spy.wait(3000));
    QVERIFY(forwarded);
    QCOMPARE(spy.first().first().toStringList(), QStringList { QStringLiteral("/tmp/x.md") });
}

void TestSingleInstance::noPrimaryMeansNoForward()
{
    SingleInstance lonely(QStringLiteral("omnimark-test-none-") + QUuid::createUuid().toString(QUuid::Id128));
    QVERIFY(!lonely.forwardToPrimary({ QStringLiteral("/tmp/x.md") }, 200));
}

QTEST_GUILESS_MAIN(TestSingleInstance)
#include "tst_singleinstance.moc"
