#ifndef PAGE_COUNTER_WIDGET_H
#define PAGE_COUNTER_WIDGET_H

#include "info_box_widget.h"

class QAbstractAnimation;

class PageCounterWidget : public InfoBoxWidget
{
    Q_OBJECT
public:
    explicit PageCounterWidget(QWidget *parent);

    // shows the text for a moment
    void flash(const QString &text);
    // Keeps a counter on screen current, such as a double page spread that is only known once both of its
    // pages are loaded. A hidden counter stays hidden.
    void refresh(const QString &text);

private:
    void showText(const QString &text);

    QAbstractAnimation *timeOnScreen;
};

#endif
