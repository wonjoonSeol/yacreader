#ifndef PAGE_LABEL_WIDGET_H
#define PAGE_LABEL_WIDGET_H

#include "info_box_widget.h"

class QPropertyAnimation;

class PageLabelWidget : public InfoBoxWidget
{
    Q_OBJECT
private:
    QPropertyAnimation *animation;

public:
    PageLabelWidget(QWidget *parent);

public slots:
    void show();
    void hide();
    void updatePosition();
};

#endif
