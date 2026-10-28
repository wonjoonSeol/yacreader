#ifndef TOOLBAR_OVERLAY_H
#define TOOLBAR_OVERLAY_H

#include <QObject>

#include <functional>

class QMainWindow;
class QMouseEvent;
class QToolBar;

namespace YACReader {

// While active, moving the mouse to the top of the window shows the hidden toolbar over the top of the page (the central
// widget) instead of docking it, so the page doesn't move. It hides again when the mouse moves below it.
class ToolBarOverlay : public QObject
{
    Q_OBJECT

public:
    // canReveal says whether the mouse at the top of the window may show the toolbar now
    ToolBarOverlay(QMainWindow *window, QToolBar *toolBar, std::function<bool()> canReveal);

    void setActive(bool active);
    // puts the toolbar back in the main window layout, hidden
    void dock();
    bool isOverPage() const;

signals:
    // the height at the top of the page the toolbar covers, 0 when it is docked
    void coveredHeightChanged(int height);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateForMove(const QMouseEvent *event);
    void showOverPage();

    QMainWindow *window;
    QToolBar *toolBar;
    std::function<bool()> canReveal;
};

}

#endif
