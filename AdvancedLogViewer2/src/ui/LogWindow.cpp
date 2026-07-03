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
    layout->setContentsMargins(1, 0, 1, 1);
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
        auto edge = hitTest(event->pos());
        if (edge != None) {
            m_resizing = true;
            m_resizeEdge = edge;
            m_resizeStart = event->globalPosition().toPoint();
            m_resizeGeom = geometry();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void LogWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_resizing) {
        QPoint delta = event->globalPosition().toPoint() - m_resizeStart;
        QRect newGeom = m_resizeGeom;

        if (m_resizeEdge & Right) {
            newGeom.setWidth(qMax(kMinWidth, m_resizeGeom.width() + delta.x()));
        }
        if (m_resizeEdge & Bottom) {
            newGeom.setHeight(qMax(kMinHeight, m_resizeGeom.height() + delta.y()));
        }
        if (m_resizeEdge & Left) {
            int newWidth = qMax(kMinWidth, m_resizeGeom.width() - delta.x());
            newGeom.setLeft(m_resizeGeom.right() - newWidth);
        }
        if (m_resizeEdge & Top) {
            int newHeight = qMax(kMinHeight, m_resizeGeom.height() - delta.y());
            newGeom.setTop(m_resizeGeom.bottom() - newHeight);
        }

        setGeometry(newGeom);
        event->accept();
        return;
    }

    auto edge = hitTest(event->pos());
    updateCursor(edge);
    QWidget::mouseMoveEvent(event);
}

void LogWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_resizing) {
        m_resizing = false;
        m_resizeEdge = None;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
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

void LogWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

LogWindow::ResizeEdge LogWindow::hitTest(const QPoint &pos) const
{
    int flags = None;
    if (pos.x() < kResizeMargin) {
        flags |= Left;
    }
    if (pos.x() > width() - kResizeMargin) {
        flags |= Right;
    }
    if (pos.y() < kResizeMargin) {
        flags |= Top;
    }
    if (pos.y() > height() - kResizeMargin) {
        flags |= Bottom;
    }
    return static_cast<ResizeEdge>(flags);
}

void LogWindow::updateCursor(ResizeEdge edge)
{
    switch (edge) {
    case TopLeft:
    case BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case TopRight:
    case BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case Left:
    case Right:
        setCursor(Qt::SizeHorCursor);
        break;
    case Top:
    case Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    default:
        unsetCursor();
        break;
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
