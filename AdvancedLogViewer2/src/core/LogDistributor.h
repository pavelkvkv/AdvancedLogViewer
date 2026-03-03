#pragma once

#include "FilterEngine.h"
#include "LogStore.h"

#include <QMutex>
#include <QObject>
#include <QString>

#include <vector>

class LogDistributor : public QObject {
    Q_OBJECT

public:
    explicit LogDistributor(QObject *parent = nullptr);

    struct Route {
        LogStore *store = nullptr;
        FilterEngine filter;
    };

    void addRoute(LogStore *store, const FilterEngine &filter);
    void addRoute(LogStore *store, const QString &filterExpr);
    void removeRoute(LogStore *store);
    void clearRoutes();

public slots:
    void distribute(const QString &line);
    void distributeBatch(const std::vector<QString> &lines);

private:
    std::vector<Route> m_routes;
    QMutex m_mutex;
};
