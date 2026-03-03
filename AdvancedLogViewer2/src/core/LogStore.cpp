#include "LogStore.h"

#include <QReadLocker>
#include <QWriteLocker>

LogStore::LogStore(QObject *parent)
    : QObject(parent)
{
}

LogStore::LogStore(size_t maxLines, QObject *parent)
    : QObject(parent)
    , m_maxLines(maxLines)
{
}

void LogStore::append(const QString &line)
{
    size_t from = 0;
    {
        QWriteLocker locker(&m_lock);

        if (m_chunks.empty() || m_chunks.back().size() >= kChunkSize) {
            m_chunks.emplace_back();
            m_chunks.back().reserve(kChunkSize);
        }
        m_chunks.back().push_back(line);
        ++m_totalLines;
        from = m_totalLines - m_evictedLines - 1;

        evictOldChunks();
    }
    emit linesAppended(from, 1);
}

void LogStore::appendBatch(const std::vector<QString> &lines)
{
    if (lines.empty()) {
        return;
    }

    size_t from = 0;
    size_t count = lines.size();
    {
        QWriteLocker locker(&m_lock);

        for (const auto &line : lines) {
            if (m_chunks.empty() || m_chunks.back().size() >= kChunkSize) {
                m_chunks.emplace_back();
                m_chunks.back().reserve(kChunkSize);
            }
            m_chunks.back().push_back(line);
            ++m_totalLines;
        }
        from = m_totalLines - m_evictedLines - count;

        evictOldChunks();
    }
    emit linesAppended(from, count);
}

QString LogStore::line(size_t index) const
{
    QReadLocker locker(&m_lock);

    if (index >= m_totalLines - m_evictedLines) {
        return {};
    }

    size_t chunkIdx = index / kChunkSize;
    size_t lineIdx = index % kChunkSize;

    if (chunkIdx >= m_chunks.size()) {
        return {};
    }
    if (lineIdx >= m_chunks[chunkIdx].size()) {
        return {};
    }

    return m_chunks[chunkIdx][lineIdx];
}

size_t LogStore::lineCount() const
{
    QReadLocker locker(&m_lock);
    return m_totalLines - m_evictedLines;
}

void LogStore::setMaxLines(size_t max)
{
    QWriteLocker locker(&m_lock);
    m_maxLines = max;
    evictOldChunks();
}

size_t LogStore::maxLines() const
{
    QReadLocker locker(&m_lock);
    return m_maxLines;
}

void LogStore::clear()
{
    QWriteLocker locker(&m_lock);
    m_chunks.clear();
    m_totalLines = 0;
    m_evictedLines = 0;
}

void LogStore::evictOldChunks()
{
    size_t currentLines = m_totalLines - m_evictedLines;
    while (currentLines > m_maxLines && !m_chunks.empty()) {
        size_t chunkLines = m_chunks.front().size();
        m_evictedLines += chunkLines;
        currentLines -= chunkLines;
        m_chunks.pop_front();
    }
}
