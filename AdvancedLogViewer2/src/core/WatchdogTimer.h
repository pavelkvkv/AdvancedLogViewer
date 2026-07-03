#pragma once

#include <QMutex>
#include <QObject>
#include <QString>
#include <QThread>

#include <atomic>
#include <memory>
#include <vector>

class WatchdogTimer : public QObject {
    Q_OBJECT

public:
    class Token {
    public:
        void heartbeat();

    private:
        friend class WatchdogTimer;
        explicit Token(int maxIntervalMs);

        std::atomic<int64_t> m_lastHeartbeat{0};
        int m_maxIntervalMs;
        QString m_name;
    };

    explicit WatchdogTimer(QObject *parent = nullptr);
    ~WatchdogTimer() override;

    std::shared_ptr<Token> registerComponent(const QString &name, int maxIntervalMs);

    void start();
    void stop();

    static void terminate(const QString &reason);

private:
    void run();
    void checkComponents();
    void checkMemory();

    static void writeCrashLog(const QString &reason);

    QThread m_thread;
    std::atomic<bool> m_running{false};

    mutable QMutex m_mutex;
    std::vector<std::shared_ptr<Token>> m_tokens;

    // Мониторинг памяти
    int64_t m_prevRssKb = 0;
    int m_highGrowthCount = 0;

    static constexpr int kCheckIntervalMs = 500;
    static constexpr int kMemoryCheckIntervalMs = 1000;
    static constexpr int64_t kMaxRssGrowthKbPerSec = 10 * 1024; // 10 MB/s
    static constexpr int kHighGrowthThreshold = 3;
};
