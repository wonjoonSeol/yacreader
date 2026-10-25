#include "page_counter_widget.h"

#include "page_counter.h"
#include "page_label_widget.h"

#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QToolBar>

PageCounterWidget::PageCounterWidget(QWidget *parent)
    : QWidget(parent), fontSizePx(PageLabelWidget::fontSizeFor(parent))
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setContentsMargins(fontSizePx * 2.3, fontSizePx / 2.3, fontSizePx * 2.3, fontSizePx / 2.3);

    textLabel = new QLabel(this);
    textLabel->setAlignment(Qt::AlignCenter);
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(textLabel);

    opacity = new QGraphicsOpacityEffect(this);
    // like NotificationsLabelWidget, it hides without fading on macOS
#ifndef Q_OS_MACOS
    setGraphicsEffect(opacity);
#endif
    fadeOut = new QPropertyAnimation(opacity, "opacity", this);
    fadeOut->setDuration(400);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    connect(fadeOut, &QAbstractAnimation::finished, this, &QWidget::hide);

    fadeOutDelay = new QTimer(this);
    fadeOutDelay->setSingleShot(true);
    fadeOutDelay->setInterval(1000);
    connect(fadeOutDelay, &QTimer::timeout, this, [this] { fadeOut->start(); });

    hide();
    initTheme(this);
}

void PageCounterWidget::flash(const QString &text)
{
    QList<QRect> toolBarAreas;
    const auto toolBars = window()->findChildren<QToolBar *>();
    for (const QToolBar *toolBar : toolBars) {
        if (toolBar->isVisible())
            toolBarAreas << QRect(parentWidget()->mapFromGlobal(toolBar->mapToGlobal(QPoint(0, 0))), toolBar->size());
    }
    move(0, YACReader::pageCounterTop(toolBarAreas));

    setText(text);
    fadeOut->stop();
    opacity->setOpacity(1.0);
    show();
    raise();
    fadeOutDelay->start();
}

void PageCounterWidget::setText(const QString &text)
{
    textLabel->setText(text);
    adjustSize();
}

void PageCounterWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), backgroundColor);
}

void PageCounterWidget::applyTheme(const Theme &theme)
{
    backgroundColor = theme.viewer.infoBackgroundColor;
    textLabel->setStyleSheet(theme.viewer.infoLabelQSS.arg(fontSizePx));
    update();
}
