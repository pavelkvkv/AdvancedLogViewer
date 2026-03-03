#include "LogFileWriter.h"
#include "LogStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <thread>

class TestLogFileWriter : public QObject {
    Q_OBJECT

private slots:
    void testBasicWrite()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        LogStore store;
        LogFileWriter writer(&store, QStringLiteral(""), tmpDir.path());
        writer.start();

        // Даём время на инициализацию
        QTest::qWait(50);

        for (int i = 0; i < 100; ++i) {
            store.append(QStringLiteral("line %1").arg(i));
        }

        // Ждём flush
        QTest::qWait(700);

        writer.stop();

        QFile file(writer.filePath());
        QVERIFY2(file.exists(), qPrintable(writer.filePath()));
        QVERIFY(file.open(QIODevice::ReadOnly));

        QByteArray content = file.readAll();
        QList<QByteArray> lines = content.split('\n');
        // Последняя строка пустая (после последнего \n)
        int nonEmpty = 0;
        for (const auto &l : lines) {
            if (!l.isEmpty()) {
                ++nonEmpty;
            }
        }
        QCOMPARE(nonEmpty, 100);
    }

    void testConcurrentWrite()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        LogStore store;
        LogFileWriter writer(&store, QStringLiteral("*test*"), tmpDir.path());
        writer.start();
        QTest::qWait(50);

        constexpr int totalLines = 10000;
        constexpr int numThreads = 4;
        constexpr int linesPerThread = totalLines / numThreads;

        std::vector<std::thread> threads;
        for (int t = 0; t < numThreads; ++t) {
            threads.emplace_back([&store, t]() {
                for (int i = 0; i < linesPerThread; ++i) {
                    store.append(QStringLiteral("thread%1_line%2").arg(t).arg(i));
                }
            });
        }

        for (auto &th : threads) {
            th.join();
        }

        // Ждём запись
        QTest::qWait(1500);
        writer.stop();

        QFile file(writer.filePath());
        QVERIFY(file.exists());
        QVERIFY(file.open(QIODevice::ReadOnly));

        int lineCount = 0;
        while (!file.atEnd()) {
            QByteArray line = file.readLine();
            if (!line.trimmed().isEmpty()) {
                ++lineCount;
            }
        }

        QCOMPARE(lineCount, totalLines);
    }

    void testSafeFileName()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        LogStore store;
        LogFileWriter writer(&store, QStringLiteral("test/filter:*bad?"), tmpDir.path());

        // Имя файла не содержит запрещённых символов
        QString path = writer.filePath();
        QVERIFY(!path.contains(QLatin1Char(':')));
        QVERIFY(!path.contains(QLatin1Char('?')));
        QVERIFY(path.contains(QStringLiteral("test_filter__bad_")));
    }
};

QTEST_GUILESS_MAIN(TestLogFileWriter)
#include "test_logfilewriter.moc"
