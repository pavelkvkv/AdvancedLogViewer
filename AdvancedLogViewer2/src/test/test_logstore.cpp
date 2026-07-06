#include "LogStore.h"

#include <QReadLocker>
#include <QSignalSpy>
#include <QTest>

#include <atomic>
#include <thread>
#include <vector>

class TestLogStore : public QObject {
    Q_OBJECT

private slots:
    void testAppendAndRead()
    {
        LogStore store;
        store.append(QStringLiteral("line0"));
        store.append(QStringLiteral("line1"));
        store.append(QStringLiteral("line2"));

        QCOMPARE(store.lineCount(), size_t(3));
        QCOMPARE(store.line(0), QStringLiteral("line0"));
        QCOMPARE(store.line(1), QStringLiteral("line1"));
        QCOMPARE(store.line(2), QStringLiteral("line2"));
    }

    void testAppendBatch()
    {
        LogStore store;
        std::vector<QString> batch = {
            QStringLiteral("a"),
            QStringLiteral("b"),
            QStringLiteral("c")
        };
        store.appendBatch(batch);

        QCOMPARE(store.lineCount(), size_t(3));
        QCOMPARE(store.line(0), QStringLiteral("a"));
        QCOMPARE(store.line(2), QStringLiteral("c"));
    }

    void testChunkBoundary()
    {
        LogStore store;
        for (size_t i = 0; i < LogStore::kChunkSize + 10; ++i) {
            store.append(QString::number(i));
        }

        QCOMPARE(store.lineCount(), LogStore::kChunkSize + 10);
        QCOMPARE(store.line(0), QStringLiteral("0"));
        QCOMPARE(store.line(LogStore::kChunkSize - 1),
                 QString::number(LogStore::kChunkSize - 1));
        QCOMPARE(store.line(LogStore::kChunkSize),
                 QString::number(LogStore::kChunkSize));
    }

    void testEviction()
    {
        constexpr size_t maxLines = LogStore::kChunkSize * 2;
        LogStore store(maxLines);

        // Добавляем 3 чанка — первый должен вытесниться
        for (size_t i = 0; i < LogStore::kChunkSize * 3; ++i) {
            store.append(QString::number(i));
        }

        QVERIFY(store.lineCount() <= maxLines);
        // Первая доступная строка — из второго чанка
        QCOMPARE(store.line(0),
                 QString::number(LogStore::kChunkSize));
    }

    void testClear()
    {
        LogStore store;
        store.append(QStringLiteral("data"));
        QCOMPARE(store.lineCount(), size_t(1));
        store.clear();
        QCOMPARE(store.lineCount(), size_t(0));
    }

    void testOutOfBounds()
    {
        LogStore store;
        QCOMPARE(store.line(0), QString());
        store.append(QStringLiteral("only"));
        QCOMPARE(store.line(1), QString());
        QCOMPARE(store.line(999), QString());
    }

    void testSignalEmitted()
    {
        LogStore store;
        QSignalSpy spy(&store, &LogStore::linesAppended);
        QVERIFY(spy.isValid());

        store.append(QStringLiteral("x"));
        QCOMPARE(spy.count(), 1);
        auto args = spy.takeFirst();
        QCOMPARE(args.at(0).value<size_t>(), size_t(0));
        QCOMPARE(args.at(1).value<size_t>(), size_t(1));

        std::vector<QString> batch = {QStringLiteral("a"), QStringLiteral("b")};
        store.appendBatch(batch);
        QCOMPARE(spy.count(), 1);
        args = spy.takeFirst();
        QCOMPARE(args.at(0).value<size_t>(), size_t(1));
        QCOMPARE(args.at(1).value<size_t>(), size_t(2));
    }

    void testConcurrentAppendAndRead()
    {
        LogStore store;
        constexpr int numThreads = 4;
        constexpr int linesPerThread = 5000;

        std::vector<std::thread> writers;
        for (int t = 0; t < numThreads; ++t) {
            writers.emplace_back([&store, t]() {
                for (int i = 0; i < linesPerThread; ++i) {
                    store.append(QStringLiteral("t%1_%2").arg(t).arg(i));
                }
            });
        }

        // Читаем параллельно
        std::thread reader([&store]() {
            for (int i = 0; i < 1000; ++i) {
                size_t count = store.lineCount();
                if (count > 0) {
                    store.line(0);
                    store.line(count - 1);
                }
            }
        });

        for (auto &w : writers) {
            w.join();
        }
        reader.join();

        QCOMPARE(store.lineCount(), size_t(numThreads * linesPerThread));
    }

    // Регресс на взаимную блокировку: читатель, удерживая ОДИН внешний
    // read-lock, сканирует диапазон через lineCountLocked()/lineLocked()
    // (как pollAndWrite/FilterWorker), пока писатель добивается write-lock.
    // Старый код читал под тем же локом через line()/lineCount(), которые
    // захватывали замок повторно; при ожидающем писателе это вешало
    // приложение (QReadWriteLock нерекурсивный). Тест должен завершаться,
    // а не зависать (иначе сработает таймаут ctest).
    void testConcurrentLockedScanNoDeadlock()
    {
        LogStore store;
        std::atomic<bool> stop{false};

        // Писатель: непрерывно берёт write-lock.
        std::thread writer([&store, &stop]() {
            std::vector<QString> batch(8, QStringLiteral("payload"));
            while (!stop.load()) {
                store.appendBatch(batch);
            }
        });

        // Читатель: под одним внешним read-lock читает много строк подряд
        // lock-free аксессорами — вложенного захвата быть не должно.
        for (int iter = 0; iter < 20000; ++iter) {
            QReadLocker locker(&store.lock());
            const size_t n = store.lineCountLocked();
            QString first, last;
            if (n > 0) {
                first = store.lineLocked(0);
                last = store.lineLocked(n - 1);
            }
            Q_UNUSED(first);
            Q_UNUSED(last);
        }

        stop.store(true);
        writer.join();

        QVERIFY(store.lineCount() > 0);
    }
};

QTEST_GUILESS_MAIN(TestLogStore)
#include "test_logstore.moc"
