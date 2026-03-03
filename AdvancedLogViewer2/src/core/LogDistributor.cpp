#include "LogDistributor.h"

#include <QMutexLocker>

#include <algorithm>

LogDistributor::LogDistributor(QObject *parent)
    : QObject(parent)
{
}

void LogDistributor::addRoute(LogStore *store, const FilterEngine &filter)
{
    QMutexLocker locker(&m_mutex);
    m_routes.push_back({store, filter});
}

void LogDistributor::addRoute(LogStore *store, const QString &filterExpr)
{
    addRoute(store, FilterEngine::compile(filterExpr));
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
    for (auto &route : m_routes) {
        if (route.filter.isEmpty() || route.filter.matches(line)) {
            route.store->append(line);
        }
    }
}

void LogDistributor::distributeBatch(const std::vector<QString> &lines)
{
    QMutexLocker locker(&m_mutex);

    // Для каждого маршрута собираем прошедшие фильтр строки
    for (auto &route : m_routes) {
        if (route.filter.isEmpty()) {
            route.store->appendBatch(lines);
        } else {
            std::vector<QString> matched;
            matched.reserve(lines.size());
            for (const auto &line : lines) {
                if (route.filter.matches(line)) {
                    matched.push_back(line);
                }
            }
            if (!matched.empty()) {
                route.store->appendBatch(matched);
            }
        }
    }
}
