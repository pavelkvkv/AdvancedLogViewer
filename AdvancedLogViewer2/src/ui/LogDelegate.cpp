#include "LogDelegate.h"

#include <QPainter>
#include <QFontMetrics>
#include <QApplication>

LogDelegate::LogDelegate(const QColor &textColor, QWidget *parent)
    : QStyledItemDelegate(parent)
    , m_textColor(textColor)
    , m_font(QStringLiteral("Monospace"), 10)
{
    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);
    QFontMetrics fm(m_font);
    m_lineHeight = fm.height() + 2;
}

void LogDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const
{
    painter->save();

    // Фон: чередование + выделение
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    } else {
        QColor bg = (index.row() % 2 == 0)
                        ? option.palette.base().color()
                        : option.palette.alternateBase().color();
        painter->fillRect(option.rect, bg);
    }

    // Текст
    QString text = index.data(Qt::DisplayRole).toString();
    QColor color = (option.state & QStyle::State_Selected)
                       ? option.palette.highlightedText().color()
                       : m_textColor;

    painter->setFont(m_font);
    painter->setPen(color);

    QRect textRect = option.rect.adjusted(4, 0, -4, 0);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::TextSingleLine, text);

    painter->restore();
}

QSize LogDelegate::sizeHint(const QStyleOptionViewItem & /*option*/,
                            const QModelIndex & /*index*/) const
{
    return {100, m_lineHeight};
}

void LogDelegate::setTextColor(const QColor &color)
{
    m_textColor = color;
}

void LogDelegate::setFont(const QFont &font)
{
    m_font = font;
    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);
    QFontMetrics fm(m_font);
    m_lineHeight = fm.height() + 2;
}
