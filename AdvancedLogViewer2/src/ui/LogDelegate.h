#pragma once

#include <QStyledItemDelegate>
#include <QColor>
#include <QFont>

class LogDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit LogDelegate(const QColor &textColor, QWidget *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    void setTextColor(const QColor &color);
    void setFont(const QFont &font);

private:
    QColor m_textColor;
    QFont m_font;
    int m_lineHeight = 0;
};
