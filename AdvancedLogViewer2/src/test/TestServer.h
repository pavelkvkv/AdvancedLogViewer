#pragma once

#include <QLocalServer>
#include <QLocalSocket>
#include <QMap>
#include <QObject>

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

    static QString socketPath();

private:
    void onNewConnection();
    void onReadyRead(QLocalSocket *socket);
    QString processCommand(const QString &line);

    QLocalServer *m_server = nullptr;
    QMap<QString, LogWindow *> m_windows;
};
