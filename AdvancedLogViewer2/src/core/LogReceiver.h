#pragma once

#include <QObject>
#include <QThread>
#include <QString>
#include <QByteArray>
#include <QStringDecoder>

#include "WatchdogTimer.h"

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

class QSerialPort;
class QUdpSocket;
class QTimer;

class LogReceiver : public QObject {
    Q_OBJECT

public:
    enum class ConnectionType { Uart, Udp };
    Q_ENUM(ConnectionType)

    struct Config {
        ConnectionType type = ConnectionType::Uart;
        QString primaryPort;
        QStringList fallbackPorts;
        int baudrate = 115200;
        QString encoding = QStringLiteral("utf8");
    };

    explicit LogReceiver(const Config &config, QObject *parent = nullptr);
    ~LogReceiver() override;

    void start();
    void stop();
    bool isRunning() const { return m_running.load(); }
    void setWdtToken(std::shared_ptr<WatchdogTimer::Token> token) { m_wdtToken = std::move(token); }

signals:
    void lineReceived(const QString &line);
    void batchReceived(const std::vector<QString> &lines);
    void connectionChanged(const QString &port, bool connected);
    void baudrateChanged(int newBaudrate);
    void errorOccurred(const QString &message);

private slots:
    void onUartReadyRead();
    void onUartError();
    void onUartFallbackTimeout();
    void onUdpReadyRead();
    void onUdpFallbackTimeout();

protected:
    struct ParsedLine {
        QChar level;        // D, I, W, E
        QString timestamp;  // HH:MM:SS:mmm
        QString text;
        bool valid = false;
    };

    QStringList parseLinesFromBuffer();
    ParsedLine parseBinaryLine(const QByteArray &data) const;
    QString decodeBytes(const QByteArray &data) const;

    QByteArray m_buffer;

private:
    void startUart();
    void startUdp();
    void stopInternal();

    void connectToPort(const QString &port);
    void tryNextPort();

    ParsedLine parseTextLine(const QString &rawLine) const;

    void checkBaudrateHealth(const QString &line);
    int nearestStandardBaudrate(int target) const;

    Config m_config;
    QThread m_thread;

    QSerialPort *m_serial = nullptr;
    QUdpSocket *m_udp = nullptr;
    QTimer *m_fallbackTimer = nullptr;

    std::optional<QStringDecoder> m_decoder;

    int m_currentPortIndex = -1;
    std::atomic<bool> m_running{false};
    std::shared_ptr<WatchdogTimer::Token> m_wdtToken;

    // Для детекции сбоя бодрейта
    int m_recentLineCount = 0;
    int m_garbageLineCount = 0;
    static constexpr int kBaudrateWindowSize = 200;
    static constexpr double kGarbageThreshold = 0.5;

    // Максимальный размер накопительного буфера без разделителя строк.
    static constexpr qsizetype kMaxBufferBytes = 1 << 20; // 1 МБ
};
