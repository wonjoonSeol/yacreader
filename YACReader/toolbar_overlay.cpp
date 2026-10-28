#include "toolbar_overlay.h"

#include "toolbar_over_page.h"

#include <QApplication>
#include <QMainWindow>
#include <QMouseEvent>
#include <QToolBar>

using namespace YACReader;

ToolBarOverlay::ToolBarOverlay(QMainWindow *window, QToolBar *toolBar, std::function<bool()> canReveal)
    : QObject(window), window(window), toolBar(toolBar), canReveal(std::move(canReveal))
{
}

void ToolBarOverlay::setActive(bool active)
{
    if (active == this->active)
        return;

    this->active = active;
    // Only an application filter sees every move: a move at a widget that doesn't track the mouse, like the page number label
    // or a toolbar button, goes through the application filters and then stops, and once the go-to flow's window container
    // makes the page a native window, a filter on the top-level window doesn't see the moves over the page any more.
    if (active) {
        qApp->installEventFilter(this);
    } else {
        qApp->removeEventFilter(this);
        dock();
    }
}

void ToolBarOverlay::dock()
{
    if (!isOverPage())
        return;

    // hidden first, so it never takes space in the layout
    toolBar->hide();
    window->addToolBar(toolBar);
    emit coveredHeightChanged(0);
}

bool ToolBarOverlay::isOverPage() const
{
    return window->toolBarArea(toolBar) == Qt::NoToolBarArea;
}

bool ToolBarOverlay::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseMove:
        if (watched->isWidgetType() && static_cast<QWidget *>(watched)->window() == window)
            updateForMove(static_cast<QMouseEvent *>(event));
        break;
    case QEvent::Leave: // the mouse left the window, to another screen for example
    case QEvent::WindowDeactivate: // Alt+Tab
        // opening one of the toolbar menus also makes the mouse leave the window
        if (watched == window && !QApplication::activePopupWidget())
            dock();
        break;
    case QEvent::Resize: // the next move at the top shows it again as wide as the window
    case QEvent::WindowStateChange: // the window manager can take the window out of full screen
        if (watched == window)
            dock();
        break;
    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}

void ToolBarOverlay::updateForMove(const QMouseEvent *event)
{
    const bool overPage = isOverPage();
    // shown docked by the window's own toolbar toggle
    if (!overPage && !toolBar->isHidden())
        return;

    const bool show = shouldShowToolBarOverPage({ .shown = overPage,
                                                  // the move can come from any widget of the window, and its position() is in that widget
                                                  .mouseY = window->mapFromGlobal(event->globalPosition().toPoint()).y(),
                                                  .toolBarHeight = toolBar->height(),
                                                  .canReveal = canReveal(),
                                                  .mouseButtonPressed = event->buttons() != Qt::NoButton });
    if (show == overPage)
        return;

    show ? showOverPage() : dock();
}

void ToolBarOverlay::showOverPage()
{
    // out of the main window layout the toolbar covers the top of the page instead of pushing it down
    window->removeToolBar(toolBar);
    toolBar->setGeometry(0, 0, window->width(), toolBar->sizeHint().height());
    toolBar->raise();
    toolBar->show();
    emit coveredHeightChanged(toolBar->height());
}
