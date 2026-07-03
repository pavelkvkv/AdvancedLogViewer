#include "LogWindow.h"
#include "FilterBar.h"
#include "LogDelegate.h"
#include "LogStore.h"
#include "LogViewModel.h"
#include "TitleBar.h"

#include <QApplication>
#include <QClipboard>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QProgressBar>
#include <QMouseEvent>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWindow>

LogWindow::LogWindow(LogStore *store, const WindowDef &def, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Window)
    , m_def(def)
    , m_store(store)
{
    setMinimumSize(kMinWidth, kMinHeight);
    resize(800, 600);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);

    m_model = new LogViewModel(store, this);
    m_delegate = new LogDelegate(def.textColor, this);

    auto *layout = new QVBoxLayout(this);
    // Рамка kBorder со всех сторон — зона захвата для resize (события мыши по
    // ней достаются окну, а не QListView). Верхняя рамка над TitleBar даёт
    // изменение размера сверху, не мешая перетаскиванию за заголовок.
    layout->setContentsMargins(kBorder, kBorder, kBorder, kBorder);
    layout->setSpacing(0);

    m_titleBar = new TitleBar(def.title, def.headerColor, this);
    layout->addWidget(m_titleBar);

    m_filterBar = new FilterBar(this);
    layout->addWidget(m_filterBar);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(3);
    m_progressBar->setRange(0, 0); // indeterminate
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();
    layout->addWidget(m_progressBar);

    m_listView = new QListView(this);
    m_listView->setModel(m_model);
    m_listView->setItemDelegate(m_delegate);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_listView->setUniformItemSizes(true);
    m_listView->setAlternatingRowColors(false); // Делегат сам рисует
    m_listView->setMouseTracking(true);

    layout->addWidget(m_listView);

    // Сигналы
    connect(m_titleBar, &TitleBar::minimizeClicked, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::settingsClicked, this, &LogWindow::settingsRequested);
    connect(m_titleBar, &TitleBar::closeClicked, this, &LogWindow::onCloseClicked);
    connect(m_titleBar, &TitleBar::autoScrollToggled, this, &LogWindow::onAutoScrollToggled);

    connect(m_filterBar, &FilterBar::filterApplied, this, &LogWindow::onFilterApplied);
    connect(m_filterBar, &FilterBar::filterCleared, this, &LogWindow::onFilterCleared);
    connect(m_model, &LogViewModel::filteringStarted, m_progressBar, &QProgressBar::show);
    connect(m_model, &LogViewModel::filteringFinished, m_progressBar, &QProgressBar::hide);

    connect(m_store, &LogStore::linesAppended, this, &LogWindow::onLinesAppended,
            Qt::QueuedConnection);
}

LogWindow::~LogWindow() = default;

void LogWindow::setWindowFilter(const QString &filter)
{
    m_model->setFilter(filter);
}

void LogWindow::clearWindowFilter()
{
    m_model->clearFilter();
}

void LogWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        Qt::Edges edges = edgesAt(event->pos());
        if (edges != Qt::Edges() && windowHandle()) {
            // Изменение размера отдаём композитору (работает и на Wayland,
            // и на X11) — ручной пересчёт геометрии был источником багов.
            windowHandle()->startSystemResize(edges);
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void LogWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        setCursorForEdges(edgesAt(event->pos()));
    }
    QWidget::mouseMoveEvent(event);
}

void LogWindow::wheelEvent(QWheelEvent *event)
{
    auto *sb = m_listView->verticalScrollBar();
    int delta = event->angleDelta().y();

    if (event->modifiers() & Qt::ShiftModifier) {
        // Горизонтальная прокрутка
        auto *hsb = m_listView->horizontalScrollBar();
        hsb->setValue(hsb->value() - delta);
        event->accept();
        return;
    }

    int lines = (event->modifiers() & Qt::ControlModifier) ? kScrollFastLines : kScrollLines;
    int step = (delta > 0) ? -lines : lines;
    sb->setValue(sb->value() + step);

    // Если пользователь отскроллировал больше 3 строк от конца — отключить автопрокрутку
    int maxVal = sb->maximum();
    if (sb->value() < maxVal - 3) {
        if (m_autoScroll) {
            m_autoScroll = false;
            m_titleBar->setAutoScrollEnabled(false);
        }
    }

    event->accept();
}

void LogWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy)) {
        auto indices = m_listView->selectionModel()->selectedIndexes();
        if (!indices.isEmpty()) {
            std::sort(indices.begin(), indices.end(),
                      [](const QModelIndex &a, const QModelIndex &b) {
                          return a.row() < b.row();
                      });
            QStringList lines;
            for (const auto &idx : indices) {
                lines.append(idx.data(Qt::DisplayRole).toString());
            }
            QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
        }
        event->accept();
        return;
    }

    if (event->matches(QKeySequence::Find)) {
        if (m_filterBar->isExpanded()) {
            m_filterBar->hideAnimated();
        } else {
            m_filterBar->showAnimated();
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Q && event->modifiers() & Qt::ControlModifier) {
        QApplication::quit();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

Qt::Edges LogWindow::edgesAt(const QPoint &pos) const
{
    Qt::Edges edges;
    if (pos.x() <= kBorder) {
        edges |= Qt::LeftEdge;
    }
    if (pos.x() >= width() - kBorder) {
        edges |= Qt::RightEdge;
    }
    if (pos.y() <= kBorder) {
        edges |= Qt::TopEdge;
    }
    if (pos.y() >= height() - kBorder) {
        edges |= Qt::BottomEdge;
    }
    return edges;
}

void LogWindow::setCursorForEdges(Qt::Edges edges)
{
    const bool left = edges & Qt::LeftEdge;
    const bool right = edges & Qt::RightEdge;
    const bool top = edges & Qt::TopEdge;
    const bool bottom = edges & Qt::BottomEdge;

    if ((left && top) || (right && bottom)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if ((right && top) || (left && bottom)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (left || right) {
        setCursor(Qt::SizeHorCursor);
    } else if (top || bottom) {
        setCursor(Qt::SizeVerCursor);
    } else {
        unsetCursor();
    }
}

void LogWindow::scrollToEnd()
{
    m_listView->scrollToBottom();
}

void LogWindow::onAutoScrollToggled(bool enabled)
{
    m_autoScroll = enabled;
    if (enabled) {
        scrollToEnd();
    }
}

void LogWindow::onLinesAppended()
{
    if (m_autoScroll) {
        scrollToEnd();
    }
}

void LogWindow::onFilterApplied(const QString &text)
{
    m_model->setFilter(text);
}

void LogWindow::onFilterCleared()
{
    m_model->clearFilter();
    m_filterBar->setFilterActive(false);
}

void LogWindow::onCloseClicked()
{
    if (QApplication::keyboardModifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) {
        QApplication::quit();
    } else {
        emit closeRequested();
        close();
    }
}

bool LogWindow::event(QEvent *event)
{
    if (event->type() == QEvent::HoverMove) {
        auto *he = static_cast<QHoverEvent *>(event);
        int y = he->position().toPoint().y();
        int threshold = m_titleBar->height() + 10;
        if (y > m_titleBar->y() && y < threshold) {
            m_filterBar->showAnimated();
        }
    }
    return QWidget::event(event);
}
