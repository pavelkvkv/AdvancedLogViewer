#include "TitleBar.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

TitleBar::TitleBar(const QString &title, const QColor &headerColor,
                   QWidget *parent)
    : QWidget(parent)
    , m_headerColor(headerColor)
{
    setFixedHeight(kHeight);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 0, 0);
    layout->setSpacing(0);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "color: white; font-size: 12px; font-weight: bold;"));
    layout->addWidget(m_titleLabel);

    layout->addStretch();

    m_autoScrollBtn = makeButton(QStringLiteral("\u2193"), tr("Автопрокрутка"));
    m_autoScrollBtn->setCheckable(true);
    m_autoScrollBtn->setChecked(true);
    connect(m_autoScrollBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_autoScroll = checked;
        updateAutoScrollIcon();
        emit autoScrollToggled(checked);
    });
    layout->addWidget(m_autoScrollBtn);

    layout->addSpacing(8);

    m_minimizeBtn = makeButton(QStringLiteral("\u2212"), tr("Свернуть"));
    connect(m_minimizeBtn, &QPushButton::clicked, this, &TitleBar::minimizeClicked);
    layout->addWidget(m_minimizeBtn);

    m_settingsBtn = makeButton(QStringLiteral("\u2699"), tr("Параметры"));
    connect(m_settingsBtn, &QPushButton::clicked, this, &TitleBar::settingsClicked);
    layout->addWidget(m_settingsBtn);

    m_closeBtn = makeButton(QStringLiteral("\u2715"), tr("Закрыть"));
    connect(m_closeBtn, &QPushButton::clicked, this, &TitleBar::closeClicked);
    layout->addWidget(m_closeBtn);

    updateAutoScrollIcon();
}

void TitleBar::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

void TitleBar::setHeaderColor(const QColor &color)
{
    m_headerColor = color;
    update();
}

void TitleBar::setAutoScrollEnabled(bool enabled)
{
    m_autoScroll = enabled;
    m_autoScrollBtn->setChecked(enabled);
    updateAutoScrollIcon();
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStart = event->globalPosition().toPoint() - window()->frameGeometry().topLeft();
        event->accept();
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        window()->move(event->globalPosition().toPoint() - m_dragStart);
        event->accept();
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
    }
}

void TitleBar::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), m_headerColor);
}

void TitleBar::updateAutoScrollIcon()
{
    if (m_autoScroll) {
        m_autoScrollBtn->setText(QStringLiteral("\u21E3")); // ⇣
        m_autoScrollBtn->setToolTip(tr("Автопрокрутка вкл."));
    } else {
        m_autoScrollBtn->setText(QStringLiteral("\u2016")); // ‖
        m_autoScrollBtn->setToolTip(tr("Автопрокрутка выкл."));
    }
}

QPushButton *TitleBar::makeButton(const QString &text, const QString &tooltip)
{
    auto *btn = new QPushButton(text, this);
    btn->setFixedSize(kHeight, kHeight);
    btn->setToolTip(tooltip);
    btn->setFlat(true);
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { color: white; border: none; font-size: 14px; }"
        "QPushButton:hover { background-color: rgba(255,255,255,40); }"
        "QPushButton:pressed { background-color: rgba(255,255,255,80); }"));
    return btn;
}
