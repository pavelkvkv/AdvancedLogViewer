#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPropertyAnimation>

class FilterBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int expandedHeight READ expandedHeight WRITE setExpandedHeight)

public:
    explicit FilterBar(QWidget *parent = nullptr);

    void showAnimated();
    void hideAnimated();
    bool isExpanded() const { return m_expanded; }

    void setFilterActive(bool active);
    QString text() const;

    int expandedHeight() const { return m_expandedHeight; }
    void setExpandedHeight(int h);

signals:
    void filterApplied(const QString &text);
    void filterCleared();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QLineEdit *m_lineEdit;
    QLabel *m_indicator;
    QPropertyAnimation *m_animation;

    bool m_expanded = false;
    int m_expandedHeight = 0;

    static constexpr int kFullHeight = 24;
    static constexpr int kAnimDuration = 120;
};
