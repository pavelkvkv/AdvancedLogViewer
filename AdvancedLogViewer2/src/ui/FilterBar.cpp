#include "FilterBar.h"

#include <QHBoxLayout>
#include <QKeyEvent>

FilterBar::FilterBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(0);
    m_expandedHeight = 0;

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(4);

    m_indicator = new QLabel(this);
    m_indicator->setFixedSize(8, 8);
    m_indicator->setStyleSheet(QStringLiteral(
        "background-color: transparent; border-radius: 4px;"));
    layout->addWidget(m_indicator);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setPlaceholderText(tr("Фильтр:  ^E — уровень,  *текст* — маска,  a|b — ИЛИ"));
    m_lineEdit->setToolTip(tr(
        "^E — строка начинается с (уровень: ^D ^I ^W ^E)\n"
        "*текст* — маска (* и ?)\n"
        "startswith() endswith() contains() — функции\n"
        "a|b — любое из (ИЛИ);  пусто — показать всё"));
    m_lineEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { border: 1px solid #555; border-radius: 2px;"
        " padding: 1px 4px; font-size: 11px; }"));
    m_lineEdit->installEventFilter(this);
    layout->addWidget(m_lineEdit);

    m_animation = new QPropertyAnimation(this, "expandedHeight", this);
    m_animation->setDuration(kAnimDuration);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
}

void FilterBar::showAnimated()
{
    if (m_expanded) {
        m_lineEdit->setFocus();
        return;
    }
    m_expanded = true;
    m_animation->stop();
    m_animation->setStartValue(0);
    m_animation->setEndValue(kFullHeight);
    m_animation->start();
    m_lineEdit->setFocus();
}

void FilterBar::hideAnimated()
{
    if (!m_expanded) {
        return;
    }
    m_expanded = false;
    m_animation->stop();
    m_animation->setStartValue(kFullHeight);
    m_animation->setEndValue(0);
    m_animation->start();
}

void FilterBar::setFilterActive(bool active)
{
    if (active) {
        m_indicator->setStyleSheet(QStringLiteral(
            "background-color: #4CAF50; border-radius: 4px;"));
    } else {
        m_indicator->setStyleSheet(QStringLiteral(
            "background-color: transparent; border-radius: 4px;"));
    }
}

QString FilterBar::text() const
{
    return m_lineEdit->text();
}

void FilterBar::setExpandedHeight(int h)
{
    m_expandedHeight = h;
    setFixedHeight(h);
}

bool FilterBar::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_lineEdit && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            QString text = m_lineEdit->text().trimmed();
            if (text.isEmpty()) {
                emit filterCleared();
                setFilterActive(false);
            } else {
                emit filterApplied(text);
                setFilterActive(true);
            }
            return true;
        }
        if (ke->key() == Qt::Key_Escape) {
            hideAnimated();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
