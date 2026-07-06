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
    // При переподключении вернуть маршруты всех каналов (даже без открытых
    // окон), чтобы логи снова шли и в хранилища, и в файлы.
    for (const auto &ch : m_channels) {
        if (ch.store) {
            m_distributor->addRoute(ch.store, ch.def.globalFilter, ch.def.catchAll);
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

AppController::Channel *AppController::findChannel(const QString &id)
{
    for (auto &ch : m_channels) {
        if (ch.def.id == id) {
            return &ch;
        }
    }
    return nullptr;
}

AppController::Channel *AppController::ensureChannel(const WindowDef &def)
{
    if (Channel *existing = findChannel(def.id)) {
        return existing;
    }
    // Новый канал: хранилище + маршрут + запись в файл. Живёт независимо от
    // окна — логи (в т.ч. файловые) идут, даже если окно закрыто.
    auto *store = new LogStore(static_cast<size_t>(m_settings->maxLines()));
    if (m_distributor) {
        m_distributor->addRoute(store, def.globalFilter, def.catchAll);
    }
    LogFileWriter *writer = nullptr;
    if (!m_settings->logDir().isEmpty()) {
        writer = new LogFileWriter(store, def.globalFilter, m_settings->logDir());
        writer->start();
    }
    m_channels.push_back({def, store, writer, nullptr});
    return &m_channels.back();
}

void AppController::attachView(Channel &ch)
{
    if (ch.window) {
        ch.window->raise();
        ch.window->activateWindow();
        return;
    }

    auto *window = new LogWindow(ch.store, ch.def);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setFont(m_settings->logFont());

    // Восстановление геометрии из активного профиля.
    const auto layout = m_profileMgr->activeProfile().layout;
    for (const auto &l : layout) {
        if (l.windowId == ch.def.id && l.geometry.isValid()) {
            window->setGeometry(l.geometry);
            break;
        }
    }

    connect(window, &LogWindow::settingsRequested,
            this, &AppController::settingsRequested);
    connect(window, &LogWindow::connectionToggleRequested,
            this, &AppController::toggleConnection);
    connect(window, &QObject::destroyed, this, [this, id = ch.def.id]() {
        onWindowClosed(id);
    });
    connect(this, &AppController::connectionStateChanged,
            window, &LogWindow::setConnected);
    window->setConnected(isConnected());

    if (m_testServer) {
        m_testServer->registerWindow(window);
    }

    ch.window = window;
    window->show();
    window->raise();
    window->activateWindow();
}

void AppController::buildChannels(const Profile &profile)
{
    for (const auto &w : profile.windows) {
        ensureChannel(w); // маршруты и запись в файл поднимаются сразу
    }
}

void AppController::openWindow(const WindowDef &def)
{
    // Нужен работающий конвейер (запустит приём, если отключён/не стартовал).
    ensurePipeline(m_profileMgr->activeProfile().connection);
    Channel *ch = ensureChannel(def);
    attachView(*ch);
}

void AppController::openWindowById(const QString &id)
{
    const Profile p = m_profileMgr->activeProfile();
    for (const auto &w : p.windows) {
        if (w.id == id) {
            openWindow(w);
            return;
        }
    }
    if (Channel *ch = findChannel(id)) {
        openWindow(ch->def);
    }
}

void AppController::applyProfile(const QString &profileName)
{
    teardown();

    const Profile p = m_profileMgr->profile(profileName);
    // Отметить профиль активным, чтобы переподключение и быстрое открытие окон
    // использовали правильное соединение (важно при автозапуске из main).
    m_profileMgr->setActiveProfile(profileName);
    ensurePipeline(p.connection);

    // Каналы (хранилища + файлы) для ВСЕХ окон таблицы — логи копятся и по
    // закрытым окнам. Представления открываем для видимых.
    buildChannels(p);
    for (auto &ch : m_channels) {
        if (ch.def.visible) {
            attachView(ch);
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
    for (const auto &ch : m_channels) {
        if (ch.window) {
            WindowLayout l;
            l.windowId = ch.def.id;
            l.geometry = ch.window->geometry();
            layout.append(l);
        }
    }
    p.layout = layout;
    m_profileMgr->saveProfile(p);
    emit statusMessage(tr("Расположение сохранено"));
}

bool AppController::anyWindowOpen() const
{
    for (const auto &ch : m_channels) {
        if (ch.window) {
            return true;
        }
    }
    return false;
}

void AppController::onWindowClosed(const QString &id)
{
    // Закрывается только ПРЕДСТАВЛЕНИЕ. Канал (хранилище/маршрут/запись в файл)
    // остаётся — логи продолжают копиться и писаться на диск.
    if (Channel *ch = findChannel(id)) {
        if (m_testServer) {
            m_testServer->unregisterWindow(id);
        }
        ch->window = nullptr;
    }

    if (!anyWindowOpen()) {
        emit allWindowsClosed();
    }
}

void AppController::teardown()
{
    // Останавливаем приём до разрушения окон и хранилищ.
    if (m_receiver) {
        m_receiver->stop();
    }

    for (auto &ch : m_channels) {
        if (m_testServer) {
            m_testServer->unregisterWindow(ch.def.id);
        }
        if (ch.writer) {
            ch.writer->stop();
            delete ch.writer;
            ch.writer = nullptr;
        }
        if (ch.window) {
            disconnect(ch.window, nullptr, this, nullptr);
            delete ch.window;
            ch.window = nullptr;
        }
        if (ch.store) {
            delete ch.store;
            ch.store = nullptr;
        }
    }
    m_channels.clear();

    delete m_receiver;
    m_receiver = nullptr;
    delete m_distributor;
    m_distributor = nullptr;

    m_pipelineRunning = false;
}
