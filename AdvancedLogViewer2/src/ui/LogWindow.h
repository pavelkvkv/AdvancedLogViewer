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
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum ResizeEdge {
        None = 0,
        Left = 1,
        Right = 2,
        Top = 4,
        Bottom = 8,
        TopLeft = Top | Left,
        TopRight = Top | Right,
        BottomLeft = Bottom | Left,
        BottomRight = Bottom | Right
    };

    ResizeEdge hitTest(const QPoint &pos) const;
    void updateCursor(ResizeEdge edge);
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
    bool m_resizing = false;
    ResizeEdge m_resizeEdge = None;
    QPoint m_resizeStart;
    QRect m_resizeGeom;

    static constexpr int kResizeMargin = 5;
    static constexpr int kMinWidth = 320;
    static constexpr int kMinHeight = 200;
    static constexpr int kScrollLines = 3;
    static constexpr int kScrollFastLines = 15;
};
