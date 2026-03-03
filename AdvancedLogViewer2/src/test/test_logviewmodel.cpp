#include "LogStore.h"
#include "LogViewModel.h"

#include <QSignalSpy>
#include <QTest>

class TestLogViewModel : public QObject {
    Q_OBJECT

private slots:
    void testUnfilteredModel()
    {
        LogStore store;
        LogViewModel model(&store);

        QCOMPARE(model.rowCount(), 0);

        store.append(QStringLiteral("line0"));
        QTest::qWait(50); // Дать обработать queued signal

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), Qt::DisplayRole).toString(),
                 QStringLiteral("line0"));

        store.append(QStringLiteral("line1"));
        store.append(QStringLiteral("line2"));
        QTest::qWait(50);

        QCOMPARE(model.rowCount(), 3);
    }

    void testFilteredModel()
    {
        LogStore store;
        store.append(QStringLiteral("E: error message"));
        store.append(QStringLiteral("I: info message"));
        store.append(QStringLiteral("W: warning message"));
        store.append(QStringLiteral("E: another error"));

        LogViewModel model(&store);
        QTest::qWait(50);
        QCOMPARE(model.rowCount(), 4);

        model.setFilter(QStringLiteral("startswith(\"E: \")"));

        // Ждём завершения FilterWorker
        QTest::qWait(200);

        QVERIFY(model.isFiltered());
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), Qt::DisplayRole).toString(),
                 QStringLiteral("E: error message"));
        QCOMPARE(model.data(model.index(1), Qt::DisplayRole).toString(),
                 QStringLiteral("E: another error"));
    }

    void testClearFilter()
    {
        LogStore store;
        store.append(QStringLiteral("E: error"));
        store.append(QStringLiteral("I: info"));

        LogViewModel model(&store);
        QTest::qWait(50);

        model.setFilter(QStringLiteral("*error*"));
        QTest::qWait(200);
        QCOMPARE(model.rowCount(), 1);

        model.clearFilter();
        QVERIFY(!model.isFiltered());
        QCOMPARE(model.rowCount(), 2);
    }

    void testNewLinesWhileFiltered()
    {
        LogStore store;
        store.append(QStringLiteral("E: old error"));

        LogViewModel model(&store);
        QTest::qWait(50);

        model.setFilter(QStringLiteral("startswith(\"E: \")"));
        QTest::qWait(200);
        QCOMPARE(model.rowCount(), 1);

        // Добавляем новые строки — часть должна пройти фильтр
        store.append(QStringLiteral("I: info"));
        store.append(QStringLiteral("E: new error"));
        QTest::qWait(100);

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(1), Qt::DisplayRole).toString(),
                 QStringLiteral("E: new error"));
    }

    void testSwitchFilter()
    {
        LogStore store;
        store.append(QStringLiteral("UART init"));
        store.append(QStringLiteral("SPI read"));
        store.append(QStringLiteral("UART done"));

        LogViewModel model(&store);
        QTest::qWait(50);

        model.setFilter(QStringLiteral("*UART*"));
        QTest::qWait(200);
        QCOMPARE(model.rowCount(), 2);

        // Меняем фильтр — старый worker отменяется
        model.setFilter(QStringLiteral("*SPI*"));
        QTest::qWait(200);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), Qt::DisplayRole).toString(),
                 QStringLiteral("SPI read"));
    }

    void testEmptyFilter()
    {
        LogStore store;
        store.append(QStringLiteral("test"));

        LogViewModel model(&store);
        QTest::qWait(50);

        model.setFilter(QStringLiteral(""));
        QVERIFY(!model.isFiltered());
        QCOMPARE(model.rowCount(), 1);
    }
};

QTEST_GUILESS_MAIN(TestLogViewModel)
#include "test_logviewmodel.moc"
