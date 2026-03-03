#include "LogDistributor.h"
#include "LogStore.h"

#include <QTest>

class TestLogDistributor : public QObject {
    Q_OBJECT

private slots:
    void testDistributeToAll()
    {
        LogStore store1;
        LogStore store2;
        LogDistributor dist;

        // Пустой фильтр — принимает всё
        dist.addRoute(&store1, QStringLiteral(""));
        dist.addRoute(&store2, QStringLiteral(""));

        dist.distribute(QStringLiteral("hello"));

        QCOMPARE(store1.lineCount(), size_t(1));
        QCOMPARE(store2.lineCount(), size_t(1));
        QCOMPARE(store1.line(0), QStringLiteral("hello"));
    }

    void testDistributeFiltered()
    {
        LogStore storeAll;
        LogStore storeErrors;
        LogDistributor dist;

        dist.addRoute(&storeAll, QStringLiteral(""));
        dist.addRoute(&storeErrors, QStringLiteral("startswith(\"E: \")"));

        dist.distribute(QStringLiteral("I: info message"));
        dist.distribute(QStringLiteral("E: error message"));
        dist.distribute(QStringLiteral("D: debug message"));

        QCOMPARE(storeAll.lineCount(), size_t(3));
        QCOMPARE(storeErrors.lineCount(), size_t(1));
        QCOMPARE(storeErrors.line(0), QStringLiteral("E: error message"));
    }

    void testDistributeWildcard()
    {
        LogStore storeUart;
        LogStore storeSpi;
        LogDistributor dist;

        dist.addRoute(&storeUart, QStringLiteral("*UART*"));
        dist.addRoute(&storeSpi, QStringLiteral("*SPI*"));

        dist.distribute(QStringLiteral("UART init ok"));
        dist.distribute(QStringLiteral("SPI read done"));
        dist.distribute(QStringLiteral("UART and SPI"));

        QCOMPARE(storeUart.lineCount(), size_t(2));
        QCOMPARE(storeSpi.lineCount(), size_t(2));
    }

    void testDistributeBatch()
    {
        LogStore storeAll;
        LogStore storeErrors;
        LogDistributor dist;

        dist.addRoute(&storeAll, QStringLiteral(""));
        dist.addRoute(&storeErrors, QStringLiteral("*ERROR*"));

        std::vector<QString> batch = {
            QStringLiteral("line1 ok"),
            QStringLiteral("line2 ERROR fail"),
            QStringLiteral("line3 ok"),
            QStringLiteral("line4 ERROR again")
        };

        dist.distributeBatch(batch);

        QCOMPARE(storeAll.lineCount(), size_t(4));
        QCOMPARE(storeErrors.lineCount(), size_t(2));
    }

    void testRemoveRoute()
    {
        LogStore store1;
        LogStore store2;
        LogDistributor dist;

        dist.addRoute(&store1, QStringLiteral(""));
        dist.addRoute(&store2, QStringLiteral(""));

        dist.distribute(QStringLiteral("before"));
        QCOMPARE(store1.lineCount(), size_t(1));
        QCOMPARE(store2.lineCount(), size_t(1));

        dist.removeRoute(&store2);
        dist.distribute(QStringLiteral("after"));
        QCOMPARE(store1.lineCount(), size_t(2));
        QCOMPARE(store2.lineCount(), size_t(1));
    }

    void testClearRoutes()
    {
        LogStore store;
        LogDistributor dist;

        dist.addRoute(&store, QStringLiteral(""));
        dist.distribute(QStringLiteral("before"));
        QCOMPARE(store.lineCount(), size_t(1));

        dist.clearRoutes();
        dist.distribute(QStringLiteral("after"));
        QCOMPARE(store.lineCount(), size_t(1));
    }

    void testMultipleStoresOneMatch()
    {
        // Строка попадает в несколько LogStore
        LogStore store1;
        LogStore store2;
        LogStore store3;
        LogDistributor dist;

        dist.addRoute(&store1, QStringLiteral("*UART*"));
        dist.addRoute(&store2, QStringLiteral("*init*"));
        dist.addRoute(&store3, QStringLiteral("*SPI*"));

        dist.distribute(QStringLiteral("UART init ok"));

        QCOMPARE(store1.lineCount(), size_t(1));
        QCOMPARE(store2.lineCount(), size_t(1));
        QCOMPARE(store3.lineCount(), size_t(0));
    }
};

QTEST_GUILESS_MAIN(TestLogDistributor)
#include "test_logdistributor.moc"
