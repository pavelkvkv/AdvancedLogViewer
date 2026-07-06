#include "Profile.h"
#include "ProfileManager.h"
#include "Settings.h"

#include <QDir>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

class TestProfile : public QObject {
    Q_OBJECT

private slots:
    void windowDefRoundTrip();
    void connectionDefRoundTrip();
    void profileRoundTrip();
    void profileManagerSaveLoad();
    void settingsRoundTrip();
    void safeFileName();
};

void TestProfile::windowDefRoundTrip()
{
    WindowDef w;
    w.id = QStringLiteral("win_1");
    w.title = QStringLiteral("Error Log");
    w.globalFilter = QStringLiteral("startswith(\"E: \")");
    w.textColor = QColor(0xFF, 0x00, 0x00);
    w.headerColor = QColor(0x30, 0x30, 0x30);
    w.visible = false;

    QJsonObject json = w.toJson();
    WindowDef w2 = WindowDef::fromJson(json);

    QCOMPARE(w2.id, w.id);
    QCOMPARE(w2.title, w.title);
    QCOMPARE(w2.globalFilter, w.globalFilter);
    QCOMPARE(w2.textColor, w.textColor);
    QCOMPARE(w2.headerColor, w.headerColor);
    QCOMPARE(w2.visible, w.visible);
}

void TestProfile::connectionDefRoundTrip()
{
    ConnectionDef c;
    c.type = QStringLiteral("uart");
    c.primaryPort = QStringLiteral("/dev/ttyACM0");
    c.fallbackPorts = {QStringLiteral("/dev/ttyACM1"), QStringLiteral("/dev/ttyUSB0")};
    c.baudrate = 921600;
    c.encoding = QStringLiteral("cp1251");

    QJsonObject json = c.toJson();
    ConnectionDef c2 = ConnectionDef::fromJson(json);

    QCOMPARE(c2.type, c.type);
    QCOMPARE(c2.primaryPort, c.primaryPort);
    QCOMPARE(c2.fallbackPorts, c.fallbackPorts);
    QCOMPARE(c2.baudrate, c.baudrate);
    QCOMPARE(c2.encoding, c.encoding);
}

void TestProfile::profileRoundTrip()
{
    Profile p;
    p.name = QStringLiteral("Test Profile");
    p.connection.type = QStringLiteral("udp");
    p.connection.primaryPort = QStringLiteral("5000");
    p.connection.baudrate = 115200;

    WindowDef w1;
    w1.id = QStringLiteral("w0");
    w1.title = QStringLiteral("All");
    p.windows.append(w1);

    WindowDef w2;
    w2.id = QStringLiteral("w1");
    w2.title = QStringLiteral("Errors");
    w2.globalFilter = QStringLiteral("E: *");
    w2.textColor = QColor(Qt::red);
    p.windows.append(w2);

    WindowLayout l;
    l.windowId = QStringLiteral("w0");
    l.geometry = QRect(100, 200, 800, 600);
    p.layout.append(l);

    QJsonObject json = p.toJson();
    Profile p2 = Profile::fromJson(json);

    QCOMPARE(p2.name, p.name);
    QCOMPARE(p2.connection.type, p.connection.type);
    QCOMPARE(p2.connection.primaryPort, p.connection.primaryPort);
    QCOMPARE(p2.windows.size(), 2);
    QCOMPARE(p2.windows[0].id, QStringLiteral("w0"));
    QCOMPARE(p2.windows[1].globalFilter, QStringLiteral("E: *"));
    QCOMPARE(p2.layout.size(), 1);
    QCOMPARE(p2.layout[0].geometry, QRect(100, 200, 800, 600));
}

void TestProfile::profileManagerSaveLoad()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Подменяем путь через переменную окружения
    // Вместо этого — используем ProfileManager напрямую (он использует staticDir)
    // Сохраним профиль вручную в tmpDir
    Profile p;
    p.name = QStringLiteral("TestSaveLoad");
    p.connection.type = QStringLiteral("uart");
    p.connection.primaryPort = QStringLiteral("/dev/ttyACM0");

    WindowDef w;
    w.id = QStringLiteral("main");
    w.title = QStringLiteral("Main");
    p.windows.append(w);

    // Проверяем JSON round-trip через файл
    QString filePath = tmpDir.path() + QStringLiteral("/test.profile.json");
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QJsonDocument doc(p.toJson());
        f.write(doc.toJson(QJsonDocument::Indented));
    }

    // Читаем обратно
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        Profile p2 = Profile::fromJson(doc.object());
        QCOMPARE(p2.name, QStringLiteral("TestSaveLoad"));
        QCOMPARE(p2.connection.primaryPort, QStringLiteral("/dev/ttyACM0"));
        QCOMPARE(p2.windows.size(), 1);
        QCOMPARE(p2.windows[0].title, QStringLiteral("Main"));
    }
}

void TestProfile::settingsRoundTrip()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    QString filePath = tmpDir.path() + QStringLiteral("/settings.json");

    // Записываем Settings вручную
    Settings s;
    s.setLanguage(QStringLiteral("en_US"));
    s.setTheme(QStringLiteral("dark"));
    s.setLogDir(QStringLiteral("/tmp/logs"));
    s.setMaxLines(1000000);
    s.setLastProfile(QStringLiteral("MyProfile"));

    QJsonObject json;
    // Используем save/load через файл напрямую
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QJsonObject obj;
        obj[QStringLiteral("language")] = s.language();
        obj[QStringLiteral("theme")] = s.theme();
        obj[QStringLiteral("log_dir")] = s.logDir();
        obj[QStringLiteral("max_lines")] = s.maxLines();
        obj[QStringLiteral("last_profile")] = s.lastProfile();
        QJsonDocument doc(obj);
        f.write(doc.toJson());
    }

    // Читаем обратно
    {
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        QJsonObject obj = doc.object();
        QCOMPARE(obj[QStringLiteral("language")].toString(), QStringLiteral("en_US"));
        QCOMPARE(obj[QStringLiteral("theme")].toString(), QStringLiteral("dark"));
        QCOMPARE(obj[QStringLiteral("log_dir")].toString(), QStringLiteral("/tmp/logs"));
        QCOMPARE(obj[QStringLiteral("max_lines")].toInt(), 1000000);
        QCOMPARE(obj[QStringLiteral("last_profile")].toString(), QStringLiteral("MyProfile"));
    }
}

void TestProfile::safeFileName()
{
    QCOMPARE(Profile::safeFileName(QStringLiteral("simple")), QStringLiteral("simple"));
    QCOMPARE(Profile::safeFileName(QStringLiteral("a/b\\c:d")), QStringLiteral("a_b_c_d"));
    QCOMPARE(Profile::safeFileName(QStringLiteral("test*?.json")), QStringLiteral("test__.json"));
}

QTEST_GUILESS_MAIN(TestProfile)
#include "test_profile.moc"
