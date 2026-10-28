#include "page_counter_widget.h"

#include <QGraphicsOpacityEffect>
#include <QPauseAnimation>
#include <QPropertyAnimation>

namespace {
constexpr int shownMs = 1000;
#ifndef Q_OS_MACOS
constexpr int fadeOutMs = 400;
#endif
}

PageCounterWidget::PageCounterWidget(QWidget *parent)
    : InfoBoxWidget(parent)
{
    // clicks go through to the page, for the "click left or right half" mouse mode
    setAttribute(Qt::WA_TransparentForMouseEvents);

    // TODO check if the effects are still broken in OSX
#ifdef Q_OS_MACOS
    // like NotificationsLabelWidget, it hides without fading
    timeOnScreen = new QPauseAnimation(shownMs, this);
#else
    auto opacity = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(opacity);
    auto fadeOut = new QPropertyAnimation(opacity, "opacity", this);
    fadeOut->setDuration(shownMs + fadeOutMs);
    fadeOut->setKeyValueAt(0.0, 1.0);
    fadeOut->setKeyValueAt(static_cast<qreal>(shownMs) / (shownMs + fadeOutMs), 1.0);
    fadeOut->setKeyValueAt(1.0, 0.0);
    timeOnScreen = fadeOut;
#endif
    connect(timeOnScreen, &QAbstractAnimation::finished, this, &QWidget::hide);

    hide();
}

void PageCounterWidget::flash(const QString &text)
{
    timeOnScreen->stop();
    showText(text);
    show();
    raise();
    timeOnScreen->start();
}

void PageCounterWidget::refresh(const QString &text)
{
    if (isVisible()) {
        showText(text);
    }
}

void PageCounterWidget::showText(const QString &text)
{
    setText(text);
    adjustSize();
}
