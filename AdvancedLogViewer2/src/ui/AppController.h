#pragma once

#include "Profile.h"
#include "WatchdogTimer.h"

#include <QObject>
#include <QVector>

#include <memory>

class ProfileManager;
class Settings;
class TestServer;
class LogStore;
class LogDistributor;
class LogReceiver;
class LogFileWriter;
class LogWindow;
class QTimer;

// Оркестратор времени выполнения: по профилю поднимает конвейер
// LogReceiver → LogDistributor → LogStore[] → LogWindow[], регистрирует
// компоненты в WatchdogTimer и TestServer. Без него набор классов ядра
// не превращается в приложение.
class AppController : public QObject {
    Q_OBJECT

public:
    AppController(ProfileManager *pm, Settings *settings,
                  TestServer *testServer, QObject *parent = nullptr);
    ~AppController() override;

    bool hasOpenWindows() const { return anyWindowOpen(); }

    bool isConnected() const { return m_receiver != nullptr && m_pipelineRunning; }

public slots:
    // Полностью пересобрать конвейер по профилю и открыть все его окна.
    void applyProfile(const QString &profileName);
    // Открыть одно окно поверх текущего (или нового) конвейера.
    void openWindow(const WindowDef &def);
    // Открыть окно по id (определение берётся из профиля/канала).
    void openWindowById(const QString &id);
    // Сохранить геометрию открытых окон в активный профиль.
    void saveLayout();
    // Освободить UART/сокет, не закрывая окна (логи остаются на экране).
    void disconnectSource();
    // Заново подключиться к источнику из активного профиля.
    void reconnectSource();
    // Переключить подключение (для кнопки в заголовке окна).
    void toggleConnection();

signals:
    void settingsRequested();
    void allWindowsClosed();
    void statusMessage(const QString &text);
    // Состояние подключения к источнику (для индикации в UI).
    void connectionStateChanged(bool connected);

private:
    // Канал = постоянный приёмник логов для одного окна из таблицы профиля:
    // хранилище + маршрут + запись в файл. Живёт, пока активен профиль, и НЕ
    // разрушается при закрытии окна — логи продолжают накапливаться. LogWindow
    // (window) — лишь текущее представление канала (может быть null).
    struct Channel {
        WindowDef def;
        LogStore *store = nullptr;
        LogFileWriter *writer = nullptr;
        LogWindow *window = nullptr;
    };

    void ensurePipeline(const ConnectionDef &conn);
    void stopReceiver();
    void startWatchdog();
    void teardown();
    void buildChannels(const Profile &profile);
    Channel *findChannel(const QString &id);
    Channel *ensureChannel(const WindowDef &def);
    void attachView(Channel &ch);
    void onWindowClosed(const QString &id);
    bool anyWindowOpen() const;

    ProfileManager *m_profileMgr;
    Settings *m_settings;
    TestServer *m_testServer;

    LogDistributor *m_distributor = nullptr;
    LogReceiver *m_receiver = nullptr;
    WatchdogTimer *m_wdt = nullptr;
    QTimer *m_heartbeatTimer = nullptr;
    std::shared_ptr<WatchdogTimer::Token> m_recvToken;

    ConnectionDef m_activeConnection;
    bool m_pipelineRunning = false;

    QVector<Channel> m_channels;
};
