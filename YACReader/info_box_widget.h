#ifndef INFO_BOX_WIDGET_H
#define INFO_BOX_WIDGET_H

#include "themable.h"

#include <QWidget>

class QLabel;

// The look of the page information label, shared with the page counter
class InfoBoxWidget : public QWidget, protected Themable
{
    Q_OBJECT
public:
    explicit InfoBoxWidget(QWidget *parent);

public slots:
    void setText(const QString &text);

protected:
    void paintEvent(QPaintEvent *) override;
    void applyTheme(const Theme &theme) override;

private:
    QLabel *textLabel;
    QColor backgroundColor;
    int fontSizePx;
};

#endif
