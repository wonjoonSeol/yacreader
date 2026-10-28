#include "page_label_widget.h"

#include <QPropertyAnimation>

PageLabelWidget::PageLabelWidget(QWidget *parent)
    : InfoBoxWidget(parent)
{
    animation = new QPropertyAnimation(this, "pos");
    animation->setDuration(150);
    animation->setEndValue(QPoint((parent->geometry().size().width() - this->width()), -this->height()));

    adjustSize(); // Resize to fit content

    if (parent != nullptr)
        move(QPoint((parent->geometry().size().width() - this->width()), -this->height()));
}

void PageLabelWidget::show()
{
    if (this->pos().y() <= 0 && animation->state() != QPropertyAnimation::Running) {
        auto parent = dynamic_cast<QWidget *>(this->parent());
        if (parent == nullptr) {
            return;
        }

        QWidget::show();
        // connect(animation,SIGNAL(finished()),this,SLOT(QWidget::hide()));
        animation->disconnect();

        animation->setStartValue(QPoint((parent->geometry().size().width() - this->width()), -this->height()));
        animation->setEndValue(QPoint((parent->geometry().size().width() - this->width()), 0));
        animation->start();
    }
}

void PageLabelWidget::hide()
{
    if (this->pos().y() >= 0 && animation->state() != QPropertyAnimation::Running) {
        auto parent = dynamic_cast<QWidget *>(this->parent());
        if (parent == nullptr) {
            return;
        }
        // connect(animation,SIGNAL(finished()),this,SLOT(setHidden()));
        animation->setStartValue(QPoint((parent->geometry().size().width() - this->width()), 0));
        animation->setEndValue(QPoint((parent->geometry().size().width() - this->width()), -this->height()));
        animation->start();
    }
}

void PageLabelWidget::updatePosition()
{
    auto parent = dynamic_cast<QWidget *>(this->parent());
    if (parent == nullptr) {
        return;
    }

    animation->stop();
    if (animation->endValue().toPoint().y() == 0)
        move(QPoint((parent->geometry().size().width() - this->width()), 0));
    else
        move(QPoint((parent->geometry().size().width() - this->width()), -this->height()));
}
