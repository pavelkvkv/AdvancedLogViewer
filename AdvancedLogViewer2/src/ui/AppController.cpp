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

    // Heartbeat токенов гоняется таймером GUI-потока, а не только приходом
    // данных: иначе простаивающий (нет трафика по UART) компонент выглядел бы
    // «зависшим» и WDT завершил бы приложение. Так детектируется реальный
    // зависон event-loop, но простой не считается сбоем.
    auto recvToken = m_wdt->registerComponent(QStringLiteral("LogReceiver"), 5000);
    auto distToken = m_wdt->registerComponent(QStringLiteral("LogDistributor"), 5000);
    if (m_receiver) {
        m_receiver->setWdtToken(recvToken);
    }
    if (m_distributor) {
        m_distributor->setWdtToken(distToken);
    }

    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(1000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, [recvToken, distToken]() {
        recvToken->heartbeat();
        distToken->heartbeat();
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

    m_receiver->start();
    m_pipelineRunning = true;
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
    connect(window, &QObject::destroyed, this, [this, id = def.id]() {
        onWindowClosed(id);
    });

    if (m_testServer) {
        m_testServer->registerWindow(window);
    }

    m_windows.push_back({def.id, store, writer, window});
    window->show();
    window->raise();
    window->activateWindow();
}

void AppController::applyProfile(const QString &profileName)
{
    teardown();

    const Profile p = m_profileMgr->profile(profileName);
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
