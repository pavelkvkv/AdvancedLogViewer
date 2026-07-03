#pragma once

#include "Profile.h"

#include <QWidget>
#include <QListView>
#include <QColor>
#include <QProgressBar>

class LogStore;
class LogViewModel;
class LogDelegate;
class TitleBar;
class FilterBar;

class LogWindow : public QWidget {
    Q_OBJECT

public:
    explicit LogWindow(LogStore *store, const WindowDef &def,
                       QWidget *parent = nullptr);
    ~LogWindow() override;

    QString windowId() const { return m_def.id; }
    LogViewModel *model() const { return m_model; }

    void setWindowFilter(const QString &filter);
    void clearWindowFilter();

signals:
    void closeRequested();
    void settingsRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    // Определяет края окна под курсором (в пределах рамки kBorder).
    Qt::Edges edgesAt(const QPoint &pos) const;
    void setCursorForEdges(Qt::Edges edges);
    void scrollToEnd();

    void onAutoScrollToggled(bool enabled);
    void onLinesAppended();
    void onCloseClicked();
    void onFilterApplied(const QString &text);
    void onFilterCleared();

    bool event(QEvent *event) override;

    WindowDef m_def;
    LogStore *m_store;
    LogViewModel *m_model;
    LogDelegate *m_delegate;
    TitleBar *m_titleBar;
    FilterBar *m_filterBar;
    QProgressBar *m_progressBar;
    QListView *m_listView;

    bool m_autoScroll = true;

    // Рамка захвата для изменения размера (окно frameless): по ней проходят
    // события мыши окна, а не дочернего QListView.
    static constexpr int kBorder = 6;
    static constexpr int kMinWidth = 320;
    static constexpr int kMinHeight = 200;
    static constexpr int kScrollLines = 3;
    static constexpr int kScrollFastLines = 15;
};
