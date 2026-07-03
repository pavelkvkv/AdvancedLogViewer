#pragma once

#include "Profile.h"

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
class WatchdogTimer;
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

    bool hasOpenWindows() const { return !m_windows.isEmpty(); }

public slots:
    // Полностью пересобрать конвейер по профилю и открыть все его окна.
    void applyProfile(const QString &profileName);
    // Открыть одно окно поверх текущего (или нового) конвейера.
    void openWindow(const WindowDef &def);
    // Сохранить геометрию открытых окон в активный профиль.
    void saveLayout();

signals:
    void settingsRequested();
    void allWindowsClosed();
    void statusMessage(const QString &text);

private:
    struct WindowCtx {
        QString id;
        LogStore *store = nullptr;
        LogFileWriter *writer = nullptr;
        LogWindow *window = nullptr;
    };

    void ensurePipeline(const ConnectionDef &conn);
    void startWatchdog();
    void teardown();
    void onWindowClosed(const QString &id);

    ProfileManager *m_profileMgr;
    Settings *m_settings;
    TestServer *m_testServer;

    LogDistributor *m_distributor = nullptr;
    LogReceiver *m_receiver = nullptr;
    WatchdogTimer *m_wdt = nullptr;
    QTimer *m_heartbeatTimer = nullptr;

    ConnectionDef m_activeConnection;
    bool m_pipelineRunning = false;

    QVector<WindowCtx> m_windows;
};
