#include "WatchdogTimer.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

#include <cstdlib>

// --- Token ---

WatchdogTimer::Token::Token(int maxIntervalMs)
    : m_maxIntervalMs(maxIntervalMs)
{
    m_lastHeartbeat.store(QDateTime::currentMSecsSinceEpoch(),
                          std::memory_order_relaxed);
}

void WatchdogTimer::Token::heartbeat()
{
    m_lastHeartbeat.store(QDateTime::currentMSecsSinceEpoch(),
                          std::memory_order_relaxed);
}

// --- WatchdogTimer ---

WatchdogTimer::WatchdogTimer(QObject *parent)
    : QObject(parent)
{
}

WatchdogTimer::~WatchdogTimer()
{
    stop();
}

std::shared_ptr<WatchdogTimer::Token>
WatchdogTimer::registerComponent(const QString &name, int maxIntervalMs)
{
    QMutexLocker locker(&m_mutex);
    std::shared_ptr<Token> token(new Token(maxIntervalMs));
    token->m_name = name;
    m_tokens.push_back(token);
    return token;
}

void WatchdogTimer::start()
{
    if (m_running.load(std::memory_order_relaxed)) {
        return;
    }

    m_running.store(true, std::memory_order_relaxed);

    // Запускаем run() в отдельном потоке (без QEventLoop)
    auto *worker = QThread::create([this]() { run(); });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void WatchdogTimer::stop()
{
    m_running.store(false, std::memory_order_relaxed);
    // Поток завершится самостоятельно в течение kCheckIntervalMs
}

void WatchdogTimer::run()
{
    int memoryCheckCounter = 0;
    const int memoryCheckTicks = kMemoryCheckIntervalMs / kCheckIntervalMs;

    while (m_running.load(std::memory_order_relaxed)) {
        QThread::msleep(kCheckIntervalMs);

        checkComponents();

        ++memoryCheckCounter;
        if (memoryCheckCounter >= memoryCheckTicks) {
            memoryCheckCounter = 0;
            checkMemory();
        }
    }
}

void WatchdogTimer::checkComponents()
{
    QMutexLocker locker(&m_mutex);
    int64_t now = QDateTime::currentMSecsSinceEpoch();

    for (const auto &token : m_tokens) {
        int64_t last = token->m_lastHeartbeat.load(std::memory_order_relaxed);
        if (now - last > token->m_maxIntervalMs) {
            QString reason = QStringLiteral("Component '%1' heartbeat timeout: %2 ms > %3 ms")
                                 .arg(token->m_name)
                                 .arg(now - last)
                                 .arg(token->m_maxIntervalMs);
            locker.unlock();
            terminate(reason);
            return;
        }
    }
}

void WatchdogTimer::checkMemory()
{
    // Читаем VmRSS из /proc/self/status
    int64_t rssKb = 0;
    {
        QFile file(QStringLiteral("/proc/self/status"));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            QString line;
            while (stream.readLineInto(&line)) {
                if (line.startsWith(QLatin1String("VmRSS:"))) {
                    // "VmRSS:    12345 kB"
                    QStringList parts = line.split(QLatin1Char(' '),
                                                   Qt::SkipEmptyParts);
                    if (parts.size() >= 2) {
                        rssKb = parts[1].toLongLong();
                    }
                    break;
                }
            }
        }
    }

    // Проверяем прирост RSS
    if (m_prevRssKb > 0 && rssKb > m_prevRssKb) {
        int64_t growthKb = rssKb - m_prevRssKb;
        // Нормализуем к 1 секунде (проверка раз в kMemoryCheckIntervalMs)
        int64_t growthPerSec = growthKb * 1000 / kMemoryCheckIntervalMs;

        if (growthPerSec > kMaxRssGrowthKbPerSec) {
            ++m_highGrowthCount;
            if (m_highGrowthCount >= kHighGrowthThreshold) {
                terminate(QStringLiteral("Memory growth too fast: %1 KB/s for %2 consecutive checks")
                              .arg(growthPerSec)
                              .arg(m_highGrowthCount));
                return;
            }
        } else {
            m_highGrowthCount = 0;
        }
    } else {
        m_highGrowthCount = 0;
    }
    m_prevRssKb = rssKb;

    // Проверяем MemAvailable
    QFile meminfo(QStringLiteral("/proc/meminfo"));
    if (meminfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&meminfo);
        int64_t memTotal = 0;
        int64_t memAvailable = 0;
        QString line;
        while (stream.readLineInto(&line)) {
            if (line.startsWith(QLatin1String("MemTotal:"))) {
                QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    memTotal = parts[1].toLongLong();
                }
            } else if (line.startsWith(QLatin1String("MemAvailable:"))) {
                QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    memAvailable = parts[1].toLongLong();
                }
            }
            if (memTotal > 0 && memAvailable > 0) {
                break;
            }
        }

        if (memTotal > 0 && memAvailable > 0) {
            double pct = static_cast<double>(memAvailable) / static_cast<double>(memTotal);
            if (pct < 0.10) {
                terminate(QStringLiteral("System memory critically low: %1 KB available / %2 KB total (%3%)")
                              .arg(memAvailable)
                              .arg(memTotal)
                              .arg(static_cast<int>(pct * 100)));
            }
        }
    }
}

void WatchdogTimer::terminate(const QString &reason)
{
    qCritical("WDT terminate: %s", qPrintable(reason));
    writeCrashLog(reason);
    std::_Exit(1);
}

void WatchdogTimer::writeCrashLog(const QString &reason)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                  + QStringLiteral("/AdvancedLogViewer2");
    QDir().mkpath(dir);

    QFile file(dir + QStringLiteral("/crash.log"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString(Qt::ISODate)
            << " WDT: " << reason << "\n";
    }
}
