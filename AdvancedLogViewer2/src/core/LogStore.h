#pragma once

#include <QObject>
#include <QReadWriteLock>
#include <QString>

#include <atomic>
#include <cstddef>
#include <deque>
#include <vector>

class LogStore : public QObject {
    Q_OBJECT

public:
    static constexpr size_t kChunkSize = 4096;
    static constexpr size_t kDefaultMaxLines = 5'000'000;

    using LogChunk = std::vector<QString>;

    explicit LogStore(QObject *parent = nullptr);
    explicit LogStore(size_t maxLines, QObject *parent = nullptr);

    void append(const QString &line);
    void appendBatch(const std::vector<QString> &lines);

    QString line(size_t index) const;
    size_t lineCount() const;

    // Lock-free варианты: вызывающий ОБЯЗАН уже держать lock() для чтения.
    // Нужны, чтобы читать несколько строк под одним внешним QReadLocker без
    // повторного (вложенного) захвата замка — вложенный read-lock при
    // ожидающем писателе приводит к взаимной блокировке (QReadWriteLock
    // нерекурсивный и не даёт новым читателям войти, пока ждёт писатель).
    QString lineLocked(size_t index) const;
    size_t lineCountLocked() const;

    void setMaxLines(size_t max);
    size_t maxLines() const;

    void clear();

    QReadWriteLock &lock() const { return m_lock; }

signals:
    void linesAppended(size_t from, size_t count);

private:
    void evictOldChunks();

    std::deque<LogChunk> m_chunks;
    size_t m_totalLines = 0;
    size_t m_evictedLines = 0;
    size_t m_maxLines = kDefaultMaxLines;
    mutable QReadWriteLock m_lock;
};
