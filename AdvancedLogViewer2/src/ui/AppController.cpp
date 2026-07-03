#include "AppController.h"

#include "LogDistributor.h"
#include "LogFileWriter.h"
#include "LogReceiver.h"
#include "LogStore.h"
#include "LogWindow.h"
#include "ProfileManager.h"
#include "Settings.h"
#include "TestServer.h"
#include "WatchdogTimer.h"

#include <QMetaType>
#include <QTimer>

#include <vector>

AppController::AppController(ProfileManager *pm, Settings *settings,
                             TestServer *testServer, QObject *parent)
    : QObject(parent)
    , m_profileMgr(pm)
    , m_settings(settings)
    , m_testServer(testServer)
{
    // Требуется для очередей сигналов между потоком приёма и GUI-потоком.
    qRegisterMetaType<std::vector<QString>>("std::vector<QString>");
    qRegisterMetaType<size_t>("size_t");
}

AppController::~AppController()
{
    teardown();
}

void AppController::startWatchdog()
{
    if (m_wdt) {
        return;
    }
    m_wdt = new WatchdogTimer(this);

    // Мониторим только поток приёма (LogReceiver сам шлёт heartbeat из своего
    // потока таймером — ловим реальный зависон I/O, а не простой без трафика).
    // GUI-поток (LogDistributor/отрисовка) НЕ регистрируем: под наплывом логов
    // event-loop законно бывает занят секундами, и это не повод убивать
    // работающее приложение. От утечки памяти защищает монитор памяти WDT.
    m_recvToken = m_wdt->registerComponent(QStringLiteral("LogReceiver"), 5000);

    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(1000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, [this]() {
        // Когда источник отключён (приёмника нет) — не даём его токену
        // «протухнуть», иначе WDT завершит приложение через таймаут.
        if (!m_pipelineRunning && m_recvToken) {
            m_recvToken->heartbeat();
        }
    });
    m_heartbeatTimer->start();

    m_wdt->start();
}

void AppController::ensurePipeline(const ConnectionDef &conn)
{
    if (m_pipelineRunning) {
        return;
    }

    m_activeConnection = conn;

    m_distributor = new LogDistributor(this);
    // При переподключении вернуть маршруты уже открытых окон, чтобы новые
    // строки снова попадали в их хранилища.
    for (const auto &ctx : m_windows) {
        if (ctx.store) {
            m_distributor->addRoute(ctx.store, ctx.globalFilter);
        }
    }

    LogReceiver::Config cfg;
    cfg.type = (conn.type.compare(QLatin1String("udp"), Qt::CaseInsensitive) == 0)
                   ? LogReceiver::ConnectionType::Udp
                   : LogReceiver::ConnectionType::Uart;
    cfg.primaryPort = conn.primaryPort;
    cfg.fallbackPorts = conn.fallbackPorts;
    cfg.baudrate = conn.baudrate > 0 ? conn.baudrate : 115200;
    cfg.encoding = conn.encoding;

    m_receiver = new LogReceiver(cfg);

    connect(m_receiver, &LogReceiver::batchReceived,
            m_distributor, &LogDistributor::distributeBatch);
    connect(m_receiver, &LogReceiver::connectionChanged, this,
            [this](const QString &port, bool connected) {
                emit statusMessage(connected
                                       ? tr("Подключено: %1").arg(port)
                                       : tr("Отключено: %1").arg(port));
            });
    connect(m_receiver, &LogReceiver::errorOccurred, this,
            [this](const QString &msg) { emit statusMessage(msg); });

    startWatchdog();
    // Токен переиспользуется между пересборками конвейера — назначаем его
    // текущему приёмнику при каждом ensurePipeline.
    if (m_recvToken) {
        m_receiver->setWdtToken(m_recvToken);
    }

    m_receiver->start();
    m_pipelineRunning = true;
    emit connectionStateChanged(true);
}

void AppController::stopReceiver()
{
    if (m_receiver) {
        m_receiver->stop();
        delete m_receiver;
        m_receiver = nullptr;
    }
    if (m_distributor) {
        delete m_distributor;
        m_distributor = nullptr;
    }
    m_pipelineRunning = false;
}

void AppController::disconnectSource()
{
    if (!m_pipelineRunning) {
        return;
    }
    stopReceiver();
    emit statusMessage(tr("Источник отключён (порт освобождён)"));
    emit connectionStateChanged(false);
}

void AppController::reconnectSource()
{
    if (m_pipelineRunning) {
        return;
    }
    // Переподключаемся к тому же соединению, что использовалось (надёжнее,
    // чем перечитывать профиль). ensurePipeline вернёт маршруты открытых окон.
    ensurePipeline(m_activeConnection);
    emit statusMessage(tr("Переподключение к источнику"));
}

void AppController::toggleConnection()
{
    if (m_pipelineRunning) {
        disconnectSource();
    } else {
        reconnectSource();
    }
}

void AppController::openWindow(const WindowDef &def)
{
    // Уже открыто — просто поднять на передний план.
    for (const auto &ctx : m_windows) {
        if (ctx.id == def.id) {
            ctx.window->raise();
            ctx.window->activateWindow();
            return;
        }
    }

    const ConnectionDef conn = m_profileMgr->activeProfile().connection;
    ensurePipeline(conn);

    auto *store = new LogStore(static_cast<size_t>(m_settings->maxLines()));
    m_distributor->addRoute(store, def.globalFilter);

    LogFileWriter *writer = nullptr;
    if (!m_settings->logDir().isEmpty()) {
        writer = new LogFileWriter(store, def.globalFilter, m_settings->logDir());
        writer->start();
    }

    auto *window = new LogWindow(store, def);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setFont(m_settings->logFont());

    // Восстановление геометрии из активного профиля.
    const auto layout = m_profileMgr->activeProfile().layout;
    for (const auto &l : layout) {
        if (l.windowId == def.id && l.geometry.isValid()) {
            window->setGeometry(l.geometry);
            break;
        }
    }

    connect(window, &LogWindow::settingsRequested,
            this, &AppController::settingsRequested);
    connect(window, &LogWindow::connectionToggleRequested,
            this, &AppController::toggleConnection);
    connect(window, &QObject::destroyed, this, [this, id = def.id]() {
        onWindowClosed(id);
    });
    // Кнопка подключения в заголовке отражает общее состояние источника.
    connect(this, &AppController::connectionStateChanged,
            window, &LogWindow::setConnected);
    window->setConnected(isConnected());

    if (m_testServer) {
        m_testServer->registerWindow(window);
    }

    m_windows.push_back({def.id, def.globalFilter, store, writer, window});
    window->show();
    window->raise();
    window->activateWindow();
}

void AppController::applyProfile(const QString &profileName)
{
    teardown();

    const Profile p = m_profileMgr->profile(profileName);
    // Отметить профиль активным, чтобы переподключение и быстрое открытие окон
    // использовали правильное соединение (важно при автозапуске из main).
    m_profileMgr->setActiveProfile(profileName);
    ensurePipeline(p.connection);

    for (const auto &w : p.windows) {
        if (w.visible) {
            openWindow(w);
        }
    }
}

void AppController::saveLayout()
{
    Profile p = m_profileMgr->activeProfile();
    if (p.name.isEmpty()) {
        return;
    }

    QVector<WindowLayout> layout;
    for (const auto &ctx : m_windows) {
        if (ctx.window) {
            WindowLayout l;
            l.windowId = ctx.id;
            l.geometry = ctx.window->geometry();
            layout.append(l);
        }
    }
    p.layout = layout;
    m_profileMgr->saveProfile(p);
    emit statusMessage(tr("Расположение сохранено"));
}

void AppController::onWindowClosed(const QString &id)
{
    for (int i = 0; i < m_windows.size(); ++i) {
        if (m_windows[i].id == id) {
            if (m_testServer) {
                m_testServer->unregisterWindow(id);
            }
            if (m_windows[i].writer) {
                m_windows[i].writer->stop();
                m_windows[i].writer->deleteLater();
            }
            // Окно уже уничтожается (WA_DeleteOnClose); store отдаём вслед.
            if (m_windows[i].store) {
                m_distributor->removeRoute(m_windows[i].store);
                m_windows[i].store->deleteLater();
            }
            m_windows.remove(i);
            break;
        }
    }

    if (m_windows.isEmpty()) {
        emit allWindowsClosed();
    }
}

void AppController::teardown()
{
    // Останавливаем приём до разрушения окон и хранилищ.
    if (m_receiver) {
        m_receiver->stop();
    }

    for (auto &ctx : m_windows) {
        if (m_testServer) {
            m_testServer->unregisterWindow(ctx.id);
        }
        if (ctx.writer) {
            ctx.writer->stop();
            delete ctx.writer;
            ctx.writer = nullptr;
        }
        if (ctx.window) {
            disconnect(ctx.window, nullptr, this, nullptr);
            delete ctx.window;
            ctx.window = nullptr;
        }
        if (ctx.store) {
            delete ctx.store;
            ctx.store = nullptr;
        }
    }
    m_windows.clear();

    delete m_receiver;
    m_receiver = nullptr;
    delete m_distributor;
    m_distributor = nullptr;

    m_pipelineRunning = false;
}
