#pragma once

#include "FilterEngine.h"

#include <QObject>
#include <QVector>

#include <atomic>
#include <cstddef>

class LogStore;

using FilterIndex = QVector<size_t>;

class FilterWorker : public QObject {
    Q_OBJECT

public:
    explicit FilterWorker(LogStore *store, const FilterEngine &filter,
                          QObject *parent = nullptr);

    void cancel() { m_cancelled.store(true); }
    bool isCancelled() const { return m_cancelled.load(); }

public slots:
    void run();

signals:
    void finished(FilterIndex index);
    void cancelled();

private:
    LogStore *m_store;
    FilterEngine m_filter;
    std::atomic<bool> m_cancelled{false};

    static constexpr size_t kBlockSize = 10000;
};
