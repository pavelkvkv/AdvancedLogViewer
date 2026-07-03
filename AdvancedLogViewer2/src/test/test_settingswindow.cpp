#include "AppController.h"
#include "Profile.h"
#include "ProfileManager.h"
#include "Settings.h"
#include "SettingsWindow.h"

#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QtTest>

// Регрессионный тест на баг «ни одна кнопка не открывает окон».
// Проверяет, что клик по «Открыть выбранное» и «Запустить» реально
// поднимает окно через AppController.
class TestSettingsWindow : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void openSelectedButton_opensWindow();
    void runButton_opensProfileWindows();
    void portComboSavesDeviceName_notLabel();
    void editsSurviveProfileSwitch();

private:
    static Profile makeProfileWithWindow(const QString &name);
};

void TestSettingsWindow::initTestCase()
{
    // Изолируем профили/настройки в тестовой директории.
    QStandardPaths::setTestModeEnabled(true);
}

Profile TestSettingsWindow::makeProfileWithWindow(const QString &name)
{
    Profile p;
    p.name = name;
    p.connection.type = QStringLiteral("udp"); // без реального железа
    p.connection.primaryPort = QStringLiteral("0"); // эфемерный порт, нет данных
    WindowDef w;
    w.id = QStringLiteral("win_a");
    w.title = QStringLiteral("A");
    w.globalFilter = QString();
    w.visible = true;
    p.windows.append(w);
    return p;
}

void TestSettingsWindow::openSelectedButton_opensWindow()
{
    ProfileManager pm;
    pm.saveProfile(makeProfileWithWindow(QStringLiteral("t1")));
    pm.setActiveProfile(QStringLiteral("t1"));

    Settings settings;
    AppController controller(&pm, &settings, nullptr);
    SettingsWindow win(&pm, &settings);
    QObject::connect(&win, &SettingsWindow::windowOpenRequested,
                     &controller, &AppController::openWindow);

    auto *table = win.findChild<QTableWidget *>(QStringLiteral("windowTable"));
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 1); // профиль автоселектнулся, окно подгрузилось
    table->setCurrentCell(0, 1);

    auto *btn = win.findChild<QPushButton *>(QStringLiteral("btnOpenWindow"));
    QVERIFY(btn);

    QVERIFY(!controller.hasOpenWindows());
    btn->click();
    QCoreApplication::processEvents();
    QVERIFY(controller.hasOpenWindows()); // окно реально открылось
}

void TestSettingsWindow::runButton_opensProfileWindows()
{
    ProfileManager pm;
    pm.saveProfile(makeProfileWithWindow(QStringLiteral("t2")));
    pm.setActiveProfile(QStringLiteral("t2"));

    Settings settings;
    AppController controller(&pm, &settings, nullptr);
    SettingsWindow win(&pm, &settings);
    QObject::connect(&win, &SettingsWindow::profileApplied,
                     &controller, &AppController::applyProfile);

    auto *btnRun = win.findChild<QPushButton *>(QStringLiteral("btnRun"));
    QVERIFY(btnRun);

    QVERIFY(!controller.hasOpenWindows());
    btnRun->click(); // сохраняет, применяет профиль, открывает окна, закрывает диалог
    QCoreApplication::processEvents();
    QVERIFY(controller.hasOpenWindows());
}

void TestSettingsWindow::portComboSavesDeviceName_notLabel()
{
    ProfileManager pm;
    pm.saveProfile(makeProfileWithWindow(QStringLiteral("t3")));
    pm.setActiveProfile(QStringLiteral("t3"));

    Settings settings;
    SettingsWindow win(&pm, &settings);

    // Имитируем пункт «ttyXYZ — Some Board» с реальным именем порта в data.
    auto *port = win.findChild<QComboBox *>(QStringLiteral("primaryPort"));
    QVERIFY(port);
    port->addItem(QStringLiteral("ttyXYZ — Some Board"), QStringLiteral("ttyXYZ"));
    port->setCurrentIndex(port->count() - 1);
    QCOMPARE(port->currentText(), QStringLiteral("ttyXYZ — Some Board"));

    // Сохраняем профиль кнопкой и проверяем, что в него ушло имя устройства,
    // а не подпись с описанием.
    auto *save = win.findChild<QPushButton *>(QStringLiteral("btnSaveProfile"));
    QVERIFY(save);
    save->click();

    QCOMPARE(pm.profile(QStringLiteral("t3")).connection.primaryPort,
             QStringLiteral("ttyXYZ"));
}

void TestSettingsWindow::editsSurviveProfileSwitch()
{
    ProfileManager pm;
    pm.saveProfile(makeProfileWithWindow(QStringLiteral("A")));
    pm.saveProfile(makeProfileWithWindow(QStringLiteral("B")));
    pm.setActiveProfile(QStringLiteral("A"));

    Settings settings;
    SettingsWindow win(&pm, &settings);

    auto *list = win.findChild<QListWidget *>(QStringLiteral("profileList"));
    auto *baud = win.findChild<QComboBox *>(QStringLiteral("baudrate"));
    QVERIFY(list);
    QVERIFY(baud);

    const auto names = pm.profileNames(); // отсортированы: A, B
    int rowA = names.indexOf(QStringLiteral("A"));
    int rowB = names.indexOf(QStringLiteral("B"));
    QVERIFY(rowA >= 0 && rowB >= 0);

    // A выбран автоматически; правим бодрейт и переключаемся на B, затем назад.
    list->setCurrentRow(rowA);
    baud->setCurrentText(QStringLiteral("2000000"));
    list->setCurrentRow(rowB);
    list->setCurrentRow(rowA);

    // Правка не потерялась — сохранилась в профиль и восстановилась в редакторе.
    QCOMPARE(pm.profile(QStringLiteral("A")).connection.baudrate, 2000000);
    QCOMPARE(baud->currentText(), QStringLiteral("2000000"));
}

QTEST_MAIN(TestSettingsWindow)
#include "test_settingswindow.moc"
