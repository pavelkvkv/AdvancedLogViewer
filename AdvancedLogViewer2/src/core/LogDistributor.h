#pragma once

#include "FilterEngine.h"
#include "LogStore.h"
#include "WatchdogTimer.h"

#include <QMutex>
#include <QObject>
#include <QString>

#include <memory>
#include <vector>

class LogDistributor : public QObject {
    Q_OBJECT

public:
    explicit LogDistributor(QObject *parent = nullptr);

    struct Route {
        LogStore *store = nullptr;
        FilterEngine filter;
        // «Прочее»: получает строки, не подошедшие ни одному обычному маршруту.
        bool catchAll = false;
    };

    void setWdtToken(std::shared_ptr<WatchdogTimer::Token> token) { m_wdtToken = std::move(token); }

    void addRoute(LogStore *store, const FilterEngine &filter, bool catchAll = false);
    void addRoute(LogStore *store, const QString &filterExpr, bool catchAll = false);
    void removeRoute(LogStore *store);
    void clearRoutes();

public slots:
    void distribute(const QString &line);
    void distributeBatch(const std::vector<QString> &lines);

private:
    std::vector<Route> m_routes;
    QMutex m_mutex;
    std::shared_ptr<WatchdogTimer::Token> m_wdtToken;
};
