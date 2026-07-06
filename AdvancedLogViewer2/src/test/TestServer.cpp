#include "TestServer.h"
#include "LogViewModel.h"
#include "LogWindow.h"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>

TestServer::TestServer(QObject *parent)
    : QObject(parent)
{
}

TestServer::~TestServer()
{
    stop();
}

bool TestServer::start()
{
    if (m_server) {
        return true;
    }

    m_server = new QLocalServer(this);

    QString path = socketPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QLocalServer::removeServer(path);

    if (!m_server->listen(path)) {
        delete m_server;
        m_server = nullptr;
        return false;
    }

    connect(m_server, &QLocalServer::newConnection,
            this, &TestServer::onNewConnection);

    return true;
}

void TestServer::stop()
{
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
}

void TestServer::registerWindow(LogWindow *window)
{
    m_windows[window->windowId()] = window;
}

void TestServer::setConnectionControl(std::function<void(bool)> setConnected,
                                      std::function<bool()> isConnected)
{
    m_setConnected = std::move(setConnected);
    m_isConnected = std::move(isConnected);
}

void TestServer::setWindowOpener(std::function<void(const QString &)> opener)
{
    m_windowOpener = std::move(opener);
}

void TestServer::unregisterWindow(const QString &windowId)
{
    m_windows.remove(windowId);
}

QString TestServer::socketPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/AdvancedLogViewer2/test.sock");
}

void TestServer::onNewConnection()
{
    while (auto *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            onReadyRead(socket);
        });
        connect(socket, &QLocalSocket::disconnected,
                socket, &QObject::deleteLater);
    }
}

void TestServer::onReadyRead(QLocalSocket *socket)
{
    while (socket->canReadLine()) {
        QString line = QString::fromUtf8(socket->readLine()).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        QString response = processCommand(line);
        socket->write(response.toUtf8());
        socket->write("\n");
        socket->flush();
    }
}

QString TestServer::processCommand(const QString &line)
{
    QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return QStringLiteral("ERR: empty command");
    }

    QString cmd = parts[0].toLower();

    if (cmd == QLatin1String("quit")) {
        QApplication::quit();
        return QStringLiteral("OK");
    }

    if (cmd == QLatin1String("disconnect") || cmd == QLatin1String("connect")) {
        if (!m_setConnected) {
            return QStringLiteral("ERR: connection control unavailable");
        }
        m_setConnected(cmd == QLatin1String("connect"));
        return QStringLiteral("OK");
    }

    if (cmd == QLatin1String("connected")) {
        if (!m_isConnected) {
            return QStringLiteral("ERR: connection control unavailable");
        }
        return QStringLiteral("OK %1").arg(m_isConnected() ? 1 : 0);
    }

    if (cmd == QLatin1String("close_window")) {
        if (parts.size() < 2) {
            return QStringLiteral("ERR: usage: close_window <window_id>");
        }
        auto *win = m_windows.value(parts[1]);
        if (!win) {
            return QStringLiteral("ERR: window not found: %1").arg(parts[1]);
        }
        win->close();
        return QStringLiteral("OK");
    }

    if (cmd == QLatin1String("open_window")) {
        if (parts.size() < 2) {
            return QStringLiteral("ERR: usage: open_window <window_id>");
        }
        if (!m_windowOpener) {
            return QStringLiteral("ERR: window opener unavailable");
        }
        m_windowOpener(parts[1]);
        return QStringLiteral("OK");
    }

    if (cmd == QLatin1String("get_geometry")) {
        if (parts.size() < 2) {
            return QStringLiteral("ERR: usage: get_geometry <window_id>");
        }
        auto *win = m_windows.value(parts[1]);
        if (!win) {
            return QStringLiteral("ERR: window not found: %1").arg(parts[1]);
        }
        const QRect g = win->geometry();
        return QStringLiteral("OK %1 %2 %3 %4")
            .arg(g.x()).arg(g.y()).arg(g.width()).arg(g.height());
    }

    if (cmd == QLatin1String("get_line_count")) {
        if (parts.size() < 2) {
            return QStringLiteral("ERR: usage: get_line_count <window_id>");
        }
        auto *win = m_windows.value(parts[1]);
        if (!win) {
            return QStringLiteral("ERR: window not found: %1").arg(parts[1]);
        }
        return QStringLiteral("OK %1").arg(win->model()->rowCount());
    }

    if (cmd == QLatin1String("get_line")) {
        if (parts.size() < 3) {
            return QStringLiteral("ERR: usage: get_line <window_id> <n>");
        }
        auto *win = m_windows.value(parts[1]);
        if (!win) {
            return QStringLiteral("ERR: window not found: %1").arg(parts[1]);
        }
        int row = parts[2].toInt();
        QModelIndex idx = win->model()->index(row, 0);
        if (!idx.isValid()) {
            return QStringLiteral("ERR: invalid row: %1").arg(row);
        }
        return QStringLiteral("OK %1").arg(idx.data(Qt::DisplayRole).toString());
    }

    if (cmd == QLatin1String("set_filter")) {
        if (parts.size() < 3) {
            return QStringLiteral("ERR: usage: set_filter <window_id> <text>");
        }
        auto *win = m_windows.value(parts[1]);
        if (!win) {
            return QStringLiteral("ERR: window not found: %1").arg(parts[1]);
        }
        // Остаток после window_id — текст фильтра
        QString filter = parts.mid(2).join(QLatin1Char(' '));
        win->setWindowFilter(filter);
        return QStringLiteral("OK");
    }

    if (cmd == QLatin1String("clear_filter")) {
        if (parts.size() < 2) {
            return QStringLiteral("ERR: usage: clear_filter <window_id>");
        }
        auto *win = m_windows.value(parts[1]);
        if (!win) {
            return QStringLiteral("ERR: window not found: %1").arg(parts[1]);
        }
        win->clearWindowFilter();
        return QStringLiteral("OK");
    }

    return QStringLiteral("ERR: unknown command: %1").arg(cmd);
}
