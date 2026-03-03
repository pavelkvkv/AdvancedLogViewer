#include "LogReceiver.h"

#include <QCoreApplication>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QUdpSocket>
#include <QTimer>
#include <QNetworkDatagram>

#include <algorithm>
#include <cmath>

LogReceiver::LogReceiver(const Config &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    if (config.encoding.compare(QLatin1String("cp1251"), Qt::CaseInsensitive) == 0) {
        auto dec = QStringDecoder("windows-1251");
        if (dec.isValid()) {
            m_decoder.emplace(std::move(dec));
        }
    } else if (config.encoding.compare(QLatin1String("cp866"), Qt::CaseInsensitive) == 0) {
        auto dec = QStringDecoder("IBM866");
        if (dec.isValid()) {
            m_decoder.emplace(std::move(dec));
        }
    }
    // Если m_decoder не задан — UTF-8 через QString::fromUtf8
}

LogReceiver::~LogReceiver()
{
    stop();
}

void LogReceiver::start()
{
    if (m_running.load()) {
        return;
    }

    m_running.store(true);

    moveToThread(&m_thread);
    m_thread.start();

    QMetaObject::invokeMethod(this, [this]() {
        if (m_config.type == ConnectionType::Uart) {
            startUart();
        } else {
            startUdp();
        }
    }, Qt::QueuedConnection);
}

void LogReceiver::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);

    if (m_thread.isRunning()) {
        QMetaObject::invokeMethod(this, [this]() {
            stopInternal();
        }, Qt::BlockingQueuedConnection);

        m_thread.quit();
        m_thread.wait();

        moveToThread(QCoreApplication::instance()->thread());
    }
}

void LogReceiver::startUart()
{
    m_serial = new QSerialPort(this);

    m_fallbackTimer = new QTimer(this);
    m_fallbackTimer->setSingleShot(true);
    m_fallbackTimer->setInterval(5000);
    connect(m_fallbackTimer, &QTimer::timeout, this, &LogReceiver::onUartFallbackTimeout);

    connect(m_serial, &QSerialPort::readyRead, this, &LogReceiver::onUartReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &LogReceiver::onUartError);

    m_currentPortIndex = -1;
    connectToPort(m_config.primaryPort);
}

void LogReceiver::startUdp()
{
    m_udp = new QUdpSocket(this);

    m_fallbackTimer = new QTimer(this);
    m_fallbackTimer->setSingleShot(true);
    m_fallbackTimer->setInterval(3000);
    connect(m_fallbackTimer, &QTimer::timeout, this, &LogReceiver::onUdpFallbackTimeout);

    connect(m_udp, &QUdpSocket::readyRead, this, &LogReceiver::onUdpReadyRead);

    bool bound = m_udp->bind(QHostAddress::Any, m_config.primaryPort.toUShort());
    if (bound) {
        m_currentPortIndex = -1;
        emit connectionChanged(m_config.primaryPort, true);
        m_fallbackTimer->start();
    } else {
        m_currentPortIndex = -1;
        tryNextPort();
    }
}

void LogReceiver::stopInternal()
{
    if (m_fallbackTimer) {
        m_fallbackTimer->stop();
        delete m_fallbackTimer;
        m_fallbackTimer = nullptr;
    }
    if (m_serial) {
        m_serial->close();
        delete m_serial;
        m_serial = nullptr;
    }
    if (m_udp) {
        m_udp->close();
        delete m_udp;
        m_udp = nullptr;
    }
    m_buffer.clear();
}

void LogReceiver::connectToPort(const QString &port)
{
    if (m_serial->isOpen()) {
        m_serial->close();
    }

    m_serial->setPortName(port);
    m_serial->setBaudRate(m_config.baudrate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);

    if (m_serial->open(QIODevice::ReadOnly)) {
        emit connectionChanged(port, true);
        m_fallbackTimer->start();
    } else {
        emit errorOccurred(
            QStringLiteral("Cannot open %1: %2").arg(port, m_serial->errorString()));
        tryNextPort();
    }
}

void LogReceiver::tryNextPort()
{
    if (m_config.fallbackPorts.isEmpty()) {
        return;
    }

    m_currentPortIndex = (m_currentPortIndex + 1) % m_config.fallbackPorts.size();
    const QString &nextPort = m_config.fallbackPorts.at(m_currentPortIndex);

    if (m_config.type == ConnectionType::Uart) {
        connectToPort(nextPort);
    } else {
        // UDP: rebind
        m_udp->close();
        if (m_udp->bind(QHostAddress::Any, nextPort.toUShort())) {
            emit connectionChanged(nextPort, true);
            m_fallbackTimer->start();
        } else {
            emit errorOccurred(
                QStringLiteral("Cannot bind UDP %1").arg(nextPort));
            tryNextPort();
        }
    }
}

void LogReceiver::onUartReadyRead()
{
    m_fallbackTimer->start(); // Сбросить таймаут — данные поступают

    m_buffer.append(m_serial->readAll());

    QStringList lines = parseLinesFromBuffer();
    if (lines.isEmpty()) {
        return;
    }

    std::vector<QString> batch;
    batch.reserve(lines.size());
    for (auto &line : lines) {
        checkBaudrateHealth(line);
        batch.push_back(std::move(line));
    }

    emit batchReceived(batch);
}

void LogReceiver::onUartError()
{
    if (m_serial->error() == QSerialPort::NoError) {
        return;
    }
    emit errorOccurred(
        QStringLiteral("UART error on %1: %2")
            .arg(m_serial->portName(), m_serial->errorString()));
}

void LogReceiver::onUartFallbackTimeout()
{
    emit errorOccurred(
        QStringLiteral("No data from %1 for 5s, switching port")
            .arg(m_serial->portName()));
    tryNextPort();
}

void LogReceiver::onUdpReadyRead()
{
    m_fallbackTimer->start();

    std::vector<QString> batch;
    while (m_udp->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udp->receiveDatagram();
        QByteArray data = datagram.data();

        m_buffer.append(data);
    }

    QStringList lines = parseLinesFromBuffer();
    if (lines.isEmpty()) {
        return;
    }

    std::vector<QString> result;
    result.reserve(lines.size());
    for (auto &line : lines) {
        result.push_back(std::move(line));
    }
    emit batchReceived(result);
}

void LogReceiver::onUdpFallbackTimeout()
{
    emit errorOccurred(QStringLiteral("No UDP data, switching port"));
    tryNextPort();
}

QStringList LogReceiver::parseLinesFromBuffer()
{
    QStringList result;
    int pos = 0;

    while (pos < m_buffer.size()) {
        int nlPos = m_buffer.indexOf('\n', pos);
        if (nlPos < 0) {
            break;
        }

        QByteArray rawLine = m_buffer.mid(pos, nlPos - pos);
        pos = nlPos + 1;

        if (rawLine.isEmpty()) {
            continue;
        }

        // Проверяем бинарный формат: начинается с 0x1L (L ∈ {1..4})
        if (rawLine.size() >= 6 &&
            (static_cast<uint8_t>(rawLine.at(0)) & 0xF0) == 0x10 &&
            (static_cast<uint8_t>(rawLine.at(0)) & 0x0F) >= 1 &&
            (static_cast<uint8_t>(rawLine.at(0)) & 0x0F) <= 4) {
            auto parsed = parseBinaryLine(rawLine);
            if (parsed.valid) {
                result.append(
                    QStringLiteral("%1 (%2) %3")
                        .arg(parsed.level)
                        .arg(parsed.timestamp, parsed.text));
                continue;
            }
        }

        // Текстовый формат или raw
        QString decoded = decodeBytes(rawLine);
        result.append(decoded);
    }

    m_buffer.remove(0, pos);
    return result;
}

LogReceiver::ParsedLine LogReceiver::parseTextLine(const QString &rawLine) const
{
    ParsedLine result;
    // Формат: L (HH:MM:SS:mmm) текст
    if (rawLine.length() < 16) {
        return result;
    }

    QChar level = rawLine.at(0);
    if (level != QLatin1Char('D') && level != QLatin1Char('I') &&
        level != QLatin1Char('W') && level != QLatin1Char('E')) {
        return result;
    }

    if (rawLine.at(1) != QLatin1Char(' ') || rawLine.at(2) != QLatin1Char('(')) {
        return result;
    }

    int closeParen = rawLine.indexOf(QLatin1Char(')'), 3);
    if (closeParen < 0) {
        return result;
    }

    result.level = level;
    result.timestamp = rawLine.mid(3, closeParen - 3);
    result.text = rawLine.mid(closeParen + 2); // после ") "
    result.valid = true;
    return result;
}

LogReceiver::ParsedLine LogReceiver::parseBinaryLine(const QByteArray &data) const
{
    ParsedLine result;
    if (data.size() < 6) {
        return result;
    }

    uint8_t header = static_cast<uint8_t>(data.at(0));
    int levelNum = header & 0x0F;

    static const QChar levels[] = {
        QLatin1Char('D'), QLatin1Char('I'), QLatin1Char('W'), QLatin1Char('E')
    };
    if (levelNum < 1 || levelNum > 4) {
        return result;
    }
    result.level = levels[levelNum - 1];

    // uint32_le timestamp в мс
    uint32_t ms = static_cast<uint32_t>(
        (static_cast<uint8_t>(data.at(1))) |
        (static_cast<uint8_t>(data.at(2)) << 8) |
        (static_cast<uint8_t>(data.at(3)) << 16) |
        (static_cast<uint8_t>(data.at(4)) << 24));

    int hours = static_cast<int>((ms / 3600000) % 24);
    int mins  = static_cast<int>((ms / 60000) % 60);
    int secs  = static_cast<int>((ms / 1000) % 60);
    int msec  = static_cast<int>(ms % 1000);

    result.timestamp = QStringLiteral("%1:%2:%3:%4")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(mins, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'))
        .arg(msec, 3, 10, QLatin1Char('0'));

    // Пропускаем 0x20 (пробел) после timestamp
    int textStart = 5;
    if (data.size() > 5 && data.at(5) == 0x20) {
        textStart = 6;
    }

    result.text = decodeBytes(data.mid(textStart));
    result.valid = true;
    return result;
}

QString LogReceiver::decodeBytes(const QByteArray &data) const
{
    if (m_decoder.has_value()) {
        auto &dec = const_cast<std::optional<QStringDecoder> &>(m_decoder);
        return dec.value()(data);
    }
    return QString::fromUtf8(data);
}

void LogReceiver::checkBaudrateHealth(const QString &line)
{
    ++m_recentLineCount;

    int nonAlpha = 0;
    for (const QChar &ch : line) {
        if (!ch.isLetterOrNumber() && !ch.isSpace()) {
            ++nonAlpha;
        }
    }
    if (!line.isEmpty() && nonAlpha > line.length() / 2) {
        ++m_garbageLineCount;
    }

    if (m_recentLineCount >= kBaudrateWindowSize) {
        if (static_cast<double>(m_garbageLineCount) / m_recentLineCount > kGarbageThreshold) {
            // Попытка подобрать бодрейт
            int current = m_config.baudrate;
            int lower = nearestStandardBaudrate(static_cast<int>(current * 0.95));
            int upper = nearestStandardBaudrate(static_cast<int>(current * 1.05));

            int newBaud = (lower != current) ? lower : upper;
            if (newBaud != current && m_serial) {
                m_config.baudrate = newBaud;
                m_serial->setBaudRate(newBaud);
                emit baudrateChanged(newBaud);
            }
        }
        m_recentLineCount = 0;
        m_garbageLineCount = 0;
    }
}

int LogReceiver::nearestStandardBaudrate(int target) const
{
    QList<qint32> standard = QSerialPortInfo::standardBaudRates();
    int nearest = standard.first();
    int minDiff = std::abs(target - nearest);

    for (qint32 baud : standard) {
        int diff = std::abs(target - baud);
        if (diff < minDiff) {
            minDiff = diff;
            nearest = baud;
        }
    }
    return nearest;
}
