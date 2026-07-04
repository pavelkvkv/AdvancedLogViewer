#pragma once

#include <QLocalServer>
#include <QLocalSocket>
#include <QMap>
#include <QObject>

#include <functional>

class LogWindow;

class TestServer : public QObject {
    Q_OBJECT

public:
    explicit TestServer(QObject *parent = nullptr);
    ~TestServer() override;

    bool start();
    void stop();

    void registerWindow(LogWindow *window);
    void unregisterWindow(const QString &windowId);

    // Управление подключением к источнику (для команд disconnect/connect).
    void setConnectionControl(std::function<void(bool)> setConnected,
                              std::function<bool()> isConnected);
    // Открытие окна по id (для команды open_window).
    void setWindowOpener(std::function<void(const QString &)> opener);

    static QString socketPath();

private:
    void onNewConnection();
    void onReadyRead(QLocalSocket *socket);
    QString processCommand(const QString &line);

    QLocalServer *m_server = nullptr;
    QMap<QString, LogWindow *> m_windows;
    std::function<void(bool)> m_setConnected;
    std::function<bool()> m_isConnected;
    std::function<void(const QString &)> m_windowOpener;
};
