#pragma once

#include <QObject>
#include <QThread>
#include <QString>
#include <QFile>
#include <QTimer>

#include <atomic>
#include <cstddef>

class LogStore;

class LogFileWriter : public QObject {
    Q_OBJECT

public:
    explicit LogFileWriter(LogStore *store, const QString &globalFilter,
                           const QString &logDir, QObject *parent = nullptr);
    ~LogFileWriter() override;

    void start();
    void stop();

    QString filePath() const { return m_filePath; }

private slots:
    void pollAndWrite();

private:
    static QString safeFileName(const QString &filter);
    void ensureFileOpen();

    LogStore *m_store;
    QString m_globalFilter;
    QString m_logDir;
    QString m_filePath;

    QThread m_thread;
    QFile *m_file = nullptr;
    QTimer *m_pollTimer = nullptr;

    QByteArray m_writeBuffer;
    size_t m_lastWrittenLine = 0;
    std::atomic<bool> m_running{false};

    static constexpr int kPollIntervalMs = 500;
    static constexpr int kFlushSizeBytes = 256 * 1024;
};
