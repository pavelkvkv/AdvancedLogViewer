#include "LogViewModel.h"
#include "LogStore.h"

#include <QReadLocker>
#include <QThread>

LogViewModel::LogViewModel(LogStore *store, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
{
    // Показываем уже накопленные строки (быстрое открытие окна во время сеанса).
    m_publishedRows = m_store->lineCount();
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
        return static_cast<int>(m_filterIndex.size());
    }

    return static_cast<int>(m_publishedRows);
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
    emit filteringStarted();
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
    m_filterScanned = 0;
    // Возврат к показу всех строк — публикуем текущий размер store.
    m_publishedRows = m_store->lineCount();
    endResetModel();
}

void LogViewModel::onLinesAppended(size_t /*from*/, size_t /*count*/)
{
    // Аргументы from/count не используем: сигнал приходит через очередь и к
    // моменту обработки может отставать от реального размера LogStore. Берём
    // фактический размер и публикуем всё, что ещё не показано, — так вставка
    // всегда согласована с rowCount().
    QReadLocker locker(&m_store->lock());
    const size_t storeSize = m_store->lineCount();
    locker.unlock();

    if (!m_filtered) {
        if (storeSize <= m_publishedRows) {
            return; // новых строк нет (или произошло вытеснение)
        }
        beginInsertRows(QModelIndex(), static_cast<int>(m_publishedRows),
                        static_cast<int>(storeSize) - 1);
        m_publishedRows = storeSize;
        endInsertRows();
        return;
    }

    // Фильтр активен: досканируем хвост store от m_filterScanned и добавим
    // подходящие строки в конец индекса.
    if (storeSize <= m_filterScanned) {
        return;
    }

    QVector<size_t> newMatches;
    {
        QReadLocker l2(&m_store->lock());
        const size_t cur = m_store->lineCount();
        for (size_t i = m_filterScanned; i < cur; ++i) {
            if (m_filter.matches(m_store->line(i))) {
                newMatches.append(i);
            }
        }
        m_filterScanned = cur;
    }

    if (!newMatches.isEmpty()) {
        int first = static_cast<int>(m_filterIndex.size());
        int last = first + static_cast<int>(newMatches.size()) - 1;
        beginInsertRows(QModelIndex(), first, last);
        m_filterIndex.append(newMatches);
        endInsertRows();
    }
}

void LogViewModel::onFilterFinished(FilterIndex index)
{
    beginResetModel();
    m_filterIndex = std::move(index);
    // Воркер просканировал все строки, присутствовавшие на момент завершения.
    m_filterScanned = m_store->lineCount();
    endResetModel();
    m_currentWorker = nullptr;
    emit filteringFinished();
}

void LogViewModel::onFilterCancelled()
{
    m_currentWorker = nullptr;
    emit filteringFinished();
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
