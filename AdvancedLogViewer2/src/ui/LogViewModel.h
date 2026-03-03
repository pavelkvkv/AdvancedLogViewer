#pragma once

#include "FilterEngine.h"
#include "FilterWorker.h"

#include <QAbstractListModel>

class LogStore;

class LogViewModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit LogViewModel(LogStore *store, QObject *parent = nullptr);
    ~LogViewModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setFilter(const QString &filterExpr);
    void clearFilter();
    bool isFiltered() const { return m_filtered; }

    QString filterExpression() const { return m_filterExpr; }

public slots:
    void onLinesAppended(size_t from, size_t count);

private slots:
    void onFilterFinished(FilterIndex index);
    void onFilterCancelled();

private:
    void startFilterWorker();
    void cancelCurrentWorker();

    LogStore *m_store;
    bool m_filtered = false;
    QString m_filterExpr;
    FilterEngine m_filter;
    FilterIndex m_filterIndex;

    FilterWorker *m_currentWorker = nullptr;
};
