#ifndef MOUSE_HANDLER_H
#define MOUSE_HANDLER_H

#include <QMouseEvent>

class Viewer;

namespace YACReader {

// moving the mouse into a band this high at the bottom of the viewer opens the go-to flow, and at the top of the screen in full screen shows the toolbar
constexpr int mouseOverActivationHeight = 15;

class MouseHandler
{
public:
    MouseHandler(Viewer *viewer);

    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);

private:
    Viewer *viewer;
    QPointF dragOrigin;
    QPointF dragLatestPosition;
};
}

#endif // MOUSE_HANDLER_H
