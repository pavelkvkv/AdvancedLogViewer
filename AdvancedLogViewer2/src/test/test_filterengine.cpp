#include "FilterEngine.h"

#include <QTest>

class TestFilterEngine : public QObject {
    Q_OBJECT

private slots:
    void testEmptyFilter()
    {
        FilterEngine f(QStringLiteral(""));
        QVERIFY(f.isValid());
        QVERIFY(f.isEmpty());
        QVERIFY(f.matches(QStringLiteral("anything")));
        QVERIFY(f.matches(QString()));
    }

    void testContainsSubstring()
    {
        FilterEngine f(QStringLiteral("error"));
        QVERIFY(f.isValid());
        QVERIFY(f.matches(QStringLiteral("An ERROR occurred")));
        QVERIFY(f.matches(QStringLiteral("error")));
        QVERIFY(!f.matches(QStringLiteral("all good")));
    }

    void testCaseInsensitive()
    {
        FilterEngine f(QStringLiteral("Warning"));
        QVERIFY(f.matches(QStringLiteral("WARNING message")));
        QVERIFY(f.matches(QStringLiteral("warning message")));
        QVERIFY(f.matches(QStringLiteral("A Warning!")));
    }

    void testWildcardStar()
    {
        FilterEngine f(QStringLiteral("*UART*"));
        QVERIFY(f.matches(QStringLiteral("UART init ok")));
        QVERIFY(f.matches(QStringLiteral("init UART done")));
        QVERIFY(!f.matches(QStringLiteral("SPI init")));
    }

    void testWildcardQuestion()
    {
        // ? = ровно один символ; E: (?:?:* → E: (X:Y:...
        FilterEngine f(QStringLiteral("E: (?:?:*"));
        QVERIFY(f.matches(QStringLiteral("E: (1:3:some text")));
        QVERIFY(!f.matches(QStringLiteral("D: (1:3:some text")));
        // 12 — два символа, не подходит под ?
        QVERIFY(!f.matches(QStringLiteral("E: (12:34:some text")));

        // Паттерн с несколькими ?
        FilterEngine f2(QStringLiteral("??:??:*"));
        QVERIFY(f2.matches(QStringLiteral("12:34:hello")));
        QVERIFY(!f2.matches(QStringLiteral("1:3:hello")));
    }

    void testStartsWith()
    {
        FilterEngine f(QStringLiteral("startswith(\"E: \")"));
        QVERIFY(f.matches(QStringLiteral("E: something bad")));
        QVERIFY(!f.matches(QStringLiteral("D: debug")));
        QVERIFY(!f.matches(QStringLiteral("  E: indented")));
    }

    void testEndsWith()
    {
        FilterEngine f(QStringLiteral("endswith(\".log\")"));
        QVERIFY(f.matches(QStringLiteral("file.log")));
        QVERIFY(!f.matches(QStringLiteral("file.txt")));
    }

    void testCaretAnchor()
    {
        // "^E" — краткая запись startswith; должна ловить только уровень E,
        // не путая с "E" в середине строки (в отличие от contains).
        FilterEngine f(QStringLiteral("^E"));
        QVERIFY(f.matches(QStringLiteral("E (00:00:01:100) error line")));
        QVERIFY(!f.matches(QStringLiteral("I (00:00:02:200) send CMD DONE")));
        QVERIFY(!f.matches(QStringLiteral("D (00:00:03:300) debug")));
    }

    void testCaretAnchorOr()
    {
        // Комбинация якорей через ИЛИ: показать W и E.
        FilterEngine f(QStringLiteral("^W | ^E"));
        QVERIFY(f.matches(QStringLiteral("W (t) warn")));
        QVERIFY(f.matches(QStringLiteral("E (t) err")));
        QVERIFY(!f.matches(QStringLiteral("I (t) info")));
        QVERIFY(!f.matches(QStringLiteral("D (t) dbg")));
    }

    void testContainsFunc()
    {
        FilterEngine f(QStringLiteral("contains(\"timeout\")"));
        QVERIFY(f.matches(QStringLiteral("Connection timeout error")));
        QVERIFY(!f.matches(QStringLiteral("Connection ok")));
    }

    void testContainsFuncSingleQuotes()
    {
        FilterEngine f(QStringLiteral("contains('timeout')"));
        QVERIFY(f.matches(QStringLiteral("Connection timeout error")));
    }

    void testOrOperator()
    {
        FilterEngine f(QStringLiteral("*UART* | *SPI*"));
        QVERIFY(f.matches(QStringLiteral("UART init")));
        QVERIFY(f.matches(QStringLiteral("SPI read")));
        QVERIFY(!f.matches(QStringLiteral("I2C write")));
    }

    void testOrStartswith()
    {
        FilterEngine f(QStringLiteral("startswith(\"W: \") | startswith(\"E: \")"));
        QVERIFY(f.matches(QStringLiteral("W: warning")));
        QVERIFY(f.matches(QStringLiteral("E: error")));
        QVERIFY(!f.matches(QStringLiteral("I: info")));
        QVERIFY(!f.matches(QStringLiteral("D: debug")));
    }

    void testSpecialRegexChars()
    {
        // Подстрока, содержащая спецсимволы regex — должна работать как literal
        FilterEngine f(QStringLiteral("a+b"));
        QVERIFY(f.matches(QStringLiteral("value a+b here")));
        QVERIFY(!f.matches(QStringLiteral("value aab here")));
    }

    void testCompileStatic()
    {
        auto f = FilterEngine::compile(QStringLiteral("test"));
        QVERIFY(f.isValid());
        QVERIFY(f.matches(QStringLiteral("this is a test")));
    }
};

QTEST_GUILESS_MAIN(TestFilterEngine)
#include "test_filterengine.moc"
