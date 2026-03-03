#include "LogReceiver.h"

#include <QSignalSpy>
#include <QTest>

// Тестируем парсинг напрямую через буфер:
// создаём LogReceiver, но не вызываем start() — тестируем через подачу данных
// через внутренние механизмы. Для этого делаем наследника с доступом.

class TestableReceiver : public LogReceiver {
public:
    using LogReceiver::LogReceiver;

    // Имитируем поступление данных в буфер и парсинг
    QStringList feedAndParse(const QByteArray &data)
    {
        m_buffer.append(data);
        return parseLinesFromBuffer();
    }
};

class TestLogReceiver : public QObject {
    Q_OBJECT

private slots:
    void testParseTextFormat()
    {
        LogReceiver::Config cfg;
        cfg.encoding = QStringLiteral("utf8");
        TestableReceiver receiver(cfg);

        QByteArray data = "D (01:02:03:456) Debug message\n"
                          "I (01:02:03:457) Info message\n"
                          "W (01:02:03:458) Warning message\n"
                          "E (01:02:03:459) Error message\n";

        auto lines = receiver.feedAndParse(data);
        QCOMPARE(lines.size(), 4);
        QVERIFY(lines[0].contains(QStringLiteral("Debug message")));
        QVERIFY(lines[3].contains(QStringLiteral("Error message")));
    }

    void testParseBinaryFormat()
    {
        LogReceiver::Config cfg;
        cfg.encoding = QStringLiteral("utf8");
        TestableReceiver receiver(cfg);

        // Бинарный: 0x11 (D=1) + uint32_le(3723456 = 01:02:03:456) + 0x20 + "test msg"
        QByteArray data;
        data.append(static_cast<char>(0x11)); // level 1 = D
        // 3723456 мс = 01*3600000 + 02*60000 + 03*1000 + 456
        uint32_t ms = 3723456;
        data.append(static_cast<char>(ms & 0xFF));
        data.append(static_cast<char>((ms >> 8) & 0xFF));
        data.append(static_cast<char>((ms >> 16) & 0xFF));
        data.append(static_cast<char>((ms >> 24) & 0xFF));
        data.append(static_cast<char>(0x20)); // пробел
        data.append("binary msg");
        data.append('\n');

        auto lines = receiver.feedAndParse(data);
        QCOMPARE(lines.size(), 1);
        QVERIFY(lines[0].contains(QStringLiteral("binary msg")));
        QVERIFY(lines[0].contains(QStringLiteral("01:02:03:456")));
        QVERIFY(lines[0].startsWith(QLatin1Char('D')));
    }

    void testParseBinaryLevels()
    {
        LogReceiver::Config cfg;
        cfg.encoding = QStringLiteral("utf8");
        TestableReceiver receiver(cfg);

        QChar expectedLevels[] = {QLatin1Char('D'), QLatin1Char('I'),
                                   QLatin1Char('W'), QLatin1Char('E')};

        for (int i = 0; i < 4; ++i) {
            QByteArray data;
            data.append(static_cast<char>(0x10 | (i + 1)));
            uint32_t ms = 0;
            data.append(static_cast<char>(ms & 0xFF));
            data.append(static_cast<char>((ms >> 8) & 0xFF));
            data.append(static_cast<char>((ms >> 16) & 0xFF));
            data.append(static_cast<char>((ms >> 24) & 0xFF));
            data.append(static_cast<char>(0x20));
            data.append("test");
            data.append('\n');

            auto lines = receiver.feedAndParse(data);
            QCOMPARE(lines.size(), 1);
            QVERIFY2(lines[0].at(0) == expectedLevels[i],
                      qPrintable(QStringLiteral("Level mismatch: got %1, expected %2")
                                     .arg(lines[0].at(0))
                                     .arg(expectedLevels[i])));
        }
    }

    void testParseRawLine()
    {
        LogReceiver::Config cfg;
        cfg.encoding = QStringLiteral("utf8");
        TestableReceiver receiver(cfg);

        // Строка, не соответствующая ни текстовому, ни бинарному формату
        QByteArray data = "some random text\n";
        auto lines = receiver.feedAndParse(data);
        QCOMPARE(lines.size(), 1);
        QCOMPARE(lines[0], QStringLiteral("some random text"));
    }

    void testPartialBuffer()
    {
        LogReceiver::Config cfg;
        cfg.encoding = QStringLiteral("utf8");
        TestableReceiver receiver(cfg);

        // Подаём данные без \n — строка не должна появиться
        QByteArray data = "partial line without newline";
        auto lines = receiver.feedAndParse(data);
        QCOMPARE(lines.size(), 0);

        // Теперь завершаем строку
        data = "\n";
        lines = receiver.feedAndParse(data);
        QCOMPARE(lines.size(), 1);
        QCOMPARE(lines[0], QStringLiteral("partial line without newline"));
    }

    void testMixedFormats()
    {
        LogReceiver::Config cfg;
        cfg.encoding = QStringLiteral("utf8");
        TestableReceiver receiver(cfg);

        QByteArray data;
        // Текстовая строка
        data.append("I (00:00:01:000) text line\n");

        // Бинарная строка
        data.append(static_cast<char>(0x12)); // I=2
        uint32_t ms = 1000;
        data.append(static_cast<char>(ms & 0xFF));
        data.append(static_cast<char>((ms >> 8) & 0xFF));
        data.append(static_cast<char>((ms >> 16) & 0xFF));
        data.append(static_cast<char>((ms >> 24) & 0xFF));
        data.append(static_cast<char>(0x20));
        data.append("binary line");
        data.append('\n');

        // Raw строка
        data.append("raw line\n");

        auto lines = receiver.feedAndParse(data);
        QCOMPARE(lines.size(), 3);
    }
};

QTEST_GUILESS_MAIN(TestLogReceiver)
#include "test_logreceiver.moc"
