#include "LogDistributor.h"

#include <QMutexLocker>

#include <algorithm>

LogDistributor::LogDistributor(QObject *parent)
    : QObject(parent)
{
}

void LogDistributor::addRoute(LogStore *store, const FilterEngine &filter,
                             bool catchAll)
{
    QMutexLocker locker(&m_mutex);
    m_routes.push_back({store, filter, catchAll});
}

void LogDistributor::addRoute(LogStore *store, const QString &filterExpr,
                             bool catchAll)
{
    addRoute(store, FilterEngine::compile(filterExpr), catchAll);
}

void LogDistributor::removeRoute(LogStore *store)
{
    QMutexLocker locker(&m_mutex);
    m_routes.erase(
        std::remove_if(m_routes.begin(), m_routes.end(),
                       [store](const Route &r) { return r.store == store; }),
        m_routes.end());
}

void LogDistributor::clearRoutes()
{
    QMutexLocker locker(&m_mutex);
    m_routes.clear();
}

void LogDistributor::distribute(const QString &line)
{
    QMutexLocker locker(&m_mutex);
    bool matchedNormal = false;
    for (auto &route : m_routes) {
        if (route.catchAll) {
            continue;
        }
        if (route.filter.isEmpty() || route.filter.matches(line)) {
            route.store->append(line);
            matchedNormal = true;
        }
    }
    // Строку, не подошедшую ни одному обычному маршруту, отдаём в «прочее».
    if (!matchedNormal) {
        for (auto &route : m_routes) {
            if (route.catchAll) {
                route.store->append(line);
            }
        }
    }
}

void LogDistributor::distributeBatch(const std::vector<QString> &lines)
{
    QMutexLocker locker(&m_mutex);
    if (m_wdtToken) {
        m_wdtToken->heartbeat();
    }

    bool hasCatchAll = false;
    for (const auto &route : m_routes) {
        if (route.catchAll) {
            hasCatchAll = true;
            break;
        }
    }

    // Для каждого обычного маршрута собираем подошедшие строки; параллельно
    // копим строки, не подошедшие ни одному обычному маршруту, — для «прочего».
    std::vector<std::vector<QString>> matched(m_routes.size());
    std::vector<QString> elseLines;
    for (const auto &line : lines) {
        bool matchedNormal = false;
        for (size_t i = 0; i < m_routes.size(); ++i) {
            const auto &route = m_routes[i];
            if (route.catchAll) {
                continue;
            }
            if (route.filter.isEmpty() || route.filter.matches(line)) {
                matched[i].push_back(line);
                matchedNormal = true;
            }
        }
        if (hasCatchAll && !matchedNormal) {
            elseLines.push_back(line);
        }
    }

    for (size_t i = 0; i < m_routes.size(); ++i) {
        auto &route = m_routes[i];
        if (route.catchAll) {
            if (!elseLines.empty()) {
                route.store->appendBatch(elseLines);
            }
        } else if (!matched[i].empty()) {
            route.store->appendBatch(matched[i]);
        }
    }
}
