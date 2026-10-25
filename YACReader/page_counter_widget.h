#ifndef PAGE_COUNTER_WIDGET_H
#define PAGE_COUNTER_WIDGET_H

#include "themable.h"

#include <QWidget>

class QGraphicsOpacityEffect;
class QLabel;
class QPropertyAnimation;
class QTimer;

// Shows the current pages for a moment in the top left corner of the page, with the look of the page information label
class PageCounterWidget : public QWidget, protected Themable
{
    Q_OBJECT
public:
    PageCounterWidget(QWidget *parent);

    void flash(const QString &text);
    void setText(const QString &text);

protected:
    void paintEvent(QPaintEvent *) override;
    void applyTheme(const Theme &theme) override;

private:
    QLabel *textLabel;
    QGraphicsOpacityEffect *opacity;
    QPropertyAnimation *fadeOut;
    QTimer *fadeOutDelay;
    QColor backgroundColor;
    int fontSizePx;
};

#endif
