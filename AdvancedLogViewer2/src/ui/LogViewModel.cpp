#include "LogViewModel.h"
#include "LogStore.h"

#include <QReadLocker>
#include <QThread>

LogViewModel::LogViewModel(LogStore *store, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
{
    connect(m_store, &LogStore::linesAppended,
            this, &LogViewModel::onLinesAppended,
            Qt::QueuedConnection);
}

LogViewModel::~LogViewModel()
{
    cancelCurrentWorker();
}

int LogViewModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    if (m_filtered) {
        return m_filterIndex.size();
    }

    return static_cast<int>(m_store->lineCount());
}

QVariant LogViewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole) {
        return {};
    }

    size_t lineIdx = 0;
    if (m_filtered) {
        if (index.row() >= m_filterIndex.size()) {
            return {};
        }
        lineIdx = m_filterIndex.at(index.row());
    } else {
        lineIdx = static_cast<size_t>(index.row());
    }

    return m_store->line(lineIdx);
}

void LogViewModel::setFilter(const QString &filterExpr)
{
    m_filterExpr = filterExpr;
    m_filter = FilterEngine::compile(filterExpr);

    if (filterExpr.trimmed().isEmpty() || !m_filter.isValid()) {
        clearFilter();
        return;
    }

    m_filtered = true;
    cancelCurrentWorker();
    startFilterWorker();
}

void LogViewModel::clearFilter()
{
    cancelCurrentWorker();

    beginResetModel();
    m_filtered = false;
    m_filterExpr.clear();
    m_filter = FilterEngine();
    m_filterIndex.clear();
    endResetModel();
}

void LogViewModel::onLinesAppended(size_t from, size_t count)
{
    if (!m_filtered) {
        int first = static_cast<int>(from);
        int last = static_cast<int>(from + count - 1);
        beginInsertRows(QModelIndex(), first, last);
        endInsertRows();
    } else {
        // Фильтруем новые строки и добавляем в хвост индекса
        QReadLocker locker(&m_store->lock());
        size_t storeSize = m_store->lineCount();
        size_t end = from + count;
        if (end > storeSize) {
            end = storeSize;
        }

        QVector<size_t> newMatches;
        for (size_t i = from; i < end; ++i) {
            QString line = m_store->line(i);
            if (m_filter.matches(line)) {
                newMatches.append(i);
            }
        }
        locker.unlock();

        if (!newMatches.isEmpty()) {
            int first = m_filterIndex.size();
            int last = first + newMatches.size() - 1;
            beginInsertRows(QModelIndex(), first, last);
            m_filterIndex.append(newMatches);
            endInsertRows();
        }
    }
}

void LogViewModel::onFilterFinished(FilterIndex index)
{
    beginResetModel();
    m_filterIndex = std::move(index);
    endResetModel();
    m_currentWorker = nullptr;
}

void LogViewModel::onFilterCancelled()
{
    m_currentWorker = nullptr;
}

void LogViewModel::startFilterWorker()
{
    auto *worker = new FilterWorker(m_store, m_filter);
    m_currentWorker = worker;

    auto *thread = new QThread;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &FilterWorker::run);
    connect(worker, &FilterWorker::finished,
            this, &LogViewModel::onFilterFinished,
            Qt::QueuedConnection);
    connect(worker, &FilterWorker::cancelled,
            this, &LogViewModel::onFilterCancelled,
            Qt::QueuedConnection);
    connect(worker, &FilterWorker::finished, thread, &QThread::quit);
    connect(worker, &FilterWorker::cancelled, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

void LogViewModel::cancelCurrentWorker()
{
    if (m_currentWorker) {
        m_currentWorker->cancel();
        m_currentWorker = nullptr;
    }
}
