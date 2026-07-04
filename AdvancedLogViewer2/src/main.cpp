#include "AppController.h"
#include "ProfileManager.h"
#include "Settings.h"
#include "SettingsWindow.h"
#include "TestServer.h"

#include <QApplication>
#include <QPalette>
#include <QStyleHints>
#include <QTranslator>

static void applyTheme(QApplication &app, const QString &theme)
{
    if (theme == QLatin1String("dark")) {
        QPalette p;
        p.setColor(QPalette::Window, QColor(45, 45, 45));
        p.setColor(QPalette::WindowText, QColor(208, 208, 208));
        p.setColor(QPalette::Base, QColor(30, 30, 30));
        p.setColor(QPalette::AlternateBase, QColor(50, 50, 50));
        p.setColor(QPalette::Text, QColor(208, 208, 208));
        p.setColor(QPalette::Button, QColor(55, 55, 55));
        p.setColor(QPalette::ButtonText, QColor(208, 208, 208));
        p.setColor(QPalette::Highlight, QColor(42, 130, 218));
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::ToolTipBase, QColor(60, 60, 60));
        p.setColor(QPalette::ToolTipText, QColor(208, 208, 208));
        p.setColor(QPalette::PlaceholderText, QColor(128, 128, 128));
        p.setColor(QPalette::Link, QColor(42, 130, 218));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 128, 128));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));
        app.setPalette(p);
    } else if (theme == QLatin1String("light")) {
        QPalette p;
        p.setColor(QPalette::Window, QColor(240, 240, 240));
        p.setColor(QPalette::WindowText, Qt::black);
        p.setColor(QPalette::Base, Qt::white);
        p.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
        p.setColor(QPalette::Text, Qt::black);
        p.setColor(QPalette::Button, QColor(225, 225, 225));
        p.setColor(QPalette::ButtonText, Qt::black);
        p.setColor(QPalette::Highlight, QColor(42, 130, 218));
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
        p.setColor(QPalette::ToolTipText, Qt::black);
        p.setColor(QPalette::PlaceholderText, QColor(128, 128, 128));
        p.setColor(QPalette::Link, QColor(0, 0, 255));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(160, 160, 160));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(160, 160, 160));
        app.setPalette(p);
    }
    // "system" — используем палитру по умолчанию, ничего не меняем
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("AdvancedLogViewer2"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setOrganizationName(QStringLiteral("ALV"));
    app.setQuitOnLastWindowClosed(false);

    Settings settings;
    settings.load();

    // --- i18n ---
    QTranslator translator;
    const QString lang = settings.language();
    if (lang == QLatin1String("en_US")) {
        if (translator.load(QStringLiteral(":/i18n/alv2_en.qm"))) {
            app.installTranslator(&translator);
        }
    }
    // ru_RU — исходный язык, переводчик не нужен

    // --- Тема ---
    applyTheme(app, settings.theme());

    // --- TestServer ---
    bool testMode = app.arguments().contains(QStringLiteral("--test-mode"));
    TestServer testServer;
    if (testMode) {
        testServer.start();
    }

    ProfileManager profileMgr;
    profileMgr.loadAll();

    AppController controller(&profileMgr, &settings, &testServer);
    QObject::connect(&controller, &AppController::statusMessage,
                     &app, [](const QString &m) { qInfo("[status] %s", qPrintable(m)); });
    testServer.setConnectionControl(
        [&](bool c) { if (c) controller.reconnectSource(); else controller.disconnectSource(); },
        [&]() { return controller.isConnected(); });
    testServer.setWindowOpener([&](const QString &id) { controller.openWindowById(id); });

    SettingsWindow settingsWin(&profileMgr, &settings);

    // Применение профиля — пересобрать конвейер и открыть окна профиля.
    QObject::connect(&settingsWin, &SettingsWindow::profileApplied,
                     &controller, &AppController::applyProfile);
    // Немедленное открытие одного окна из редактора.
    QObject::connect(&settingsWin, &SettingsWindow::windowOpenRequested,
                     &controller, &AppController::openWindow);
    // Кнопка «Запомнить текущее расположение».
    QObject::connect(&settingsWin, &SettingsWindow::saveLayoutRequested,
                     &controller, &AppController::saveLayout);

    // Если после применения профиля не открылось ни одного окна — вернуть
    // окно параметров, иначе приложение осталось бы невидимым (нет трея).
    QObject::connect(&settingsWin, &SettingsWindow::profileApplied,
                     &settingsWin, [&](const QString &) {
        if (!controller.hasOpenWindows()) {
            settingsWin.show();
        }
    });

    // Кнопка «Параметры» в заголовке log-окна — снова показать настройки.
    QObject::connect(&controller, &AppController::settingsRequested,
                     &settingsWin, [&]() {
        settingsWin.show();
        settingsWin.raise();
        settingsWin.activateWindow();
    });

    // Закрыто последнее log-окно и параметры не открыты — выходим.
    QObject::connect(&controller, &AppController::allWindowsClosed,
                     &app, [&]() {
        if (!settingsWin.isVisible()) {
            QApplication::quit();
        }
    });

    // Отмена в параметрах: если окна уже открыты — просто закрыть диалог,
    // иначе выходить (иначе останется невидимый процесс).
    QObject::connect(&settingsWin, &QDialog::rejected, &app, [&]() {
        if (!controller.hasOpenWindows()) {
            QApplication::quit();
        }
    });

    // Стартовое поведение: если есть последний профиль с окнами — открыть
    // сессию сразу (как в v1.3), иначе показать параметры.
    const QString lastProfile = settings.lastProfile();
    bool sessionStarted = false;
    if (!lastProfile.isEmpty() && profileMgr.hasProfile(lastProfile)) {
        const Profile p = profileMgr.profile(lastProfile);
        bool hasVisibleWindow = false;
        for (const auto &w : p.windows) {
            if (w.visible) {
                hasVisibleWindow = true;
                break;
            }
        }
        if (hasVisibleWindow) {
            controller.applyProfile(lastProfile);
            sessionStarted = controller.hasOpenWindows();
        }
    }

    if (!sessionStarted) {
        settingsWin.show();
    }

    return app.exec();
}
