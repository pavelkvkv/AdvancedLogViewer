#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QColor>

class TitleBar : public QWidget {
    Q_OBJECT

public:
    explicit TitleBar(const QString &title, const QColor &headerColor,
                      QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setHeaderColor(const QColor &color);
    void setAutoScrollEnabled(bool enabled);
    bool isAutoScrollEnabled() const { return m_autoScroll; }

    static constexpr int kHeight = 28;

signals:
    void minimizeClicked();
    void settingsClicked();
    void closeClicked();
    void autoScrollToggled(bool enabled);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void updateAutoScrollIcon();
    QPushButton *makeButton(const QString &text, const QString &tooltip);

    QLabel *m_titleLabel;
    QPushButton *m_autoScrollBtn;
    QPushButton *m_minimizeBtn;
    QPushButton *m_settingsBtn;
    QPushButton *m_closeBtn;

    QColor m_headerColor;
    bool m_autoScroll = true;
};
