#include "FilterWorker.h"
#include "LogStore.h"

#include <QReadLocker>

FilterWorker::FilterWorker(LogStore *store, const FilterEngine &filter,
                           QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_filter(filter)
{
}

void FilterWorker::run()
{
    FilterIndex index;

    QReadLocker locker(&m_store->lock());
    size_t total = m_store->lineCountLocked();
    locker.unlock();

    index.reserve(static_cast<int>(total / 10)); // Эвристика

    size_t processed = 0;
    while (processed < total) {
        if (m_cancelled.load()) {
            emit cancelled();
            return;
        }

        size_t blockEnd = qMin(processed + kBlockSize, total);

        QReadLocker blockLocker(&m_store->lock());
        // Проверяем актуальный размер — мог измениться
        size_t currentSize = m_store->lineCountLocked();
        if (currentSize > total) {
            total = currentSize;
        }
        if (blockEnd > currentSize) {
            blockEnd = currentSize;
        }

        for (size_t i = processed; i < blockEnd; ++i) {
            QString line = m_store->lineLocked(i);
            if (m_filter.matches(line)) {
                index.append(i);
            }
        }
        blockLocker.unlock();

        processed = blockEnd;
    }

    if (!m_cancelled.load()) {
        emit finished(index);
    } else {
        emit cancelled();
    }
}
