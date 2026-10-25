#ifndef TOOLBAR_OVER_PAGE_H
#define TOOLBAR_OVER_PAGE_H

#include <QMouseEvent>
#include <QWidget>

namespace YACReader {

// Full screen hides the toolbar. Moving the mouse to the top of the screen shows it over the page until the mouse leaves it.
struct ToolBarOverPageState {
    bool shown;
    int mouseY; // in the full screen window, so 0 is the top of the screen
    int toolBarHeight;
    bool magnifyingGlassShown;
    bool mouseButtonPressed;
};

constexpr int toolBarOverPageActivationHeight = 15;

constexpr bool shouldShowToolBarOverPage(const ToolBarOverPageState &state)
{
    if (state.shown)
        return state.mouseY < state.toolBarHeight;

    return state.mouseY < toolBarOverPageActivationHeight && !state.magnifyingGlassShown && !state.mouseButtonPressed;
}

// the move can come from any widget of the window, and its position() is in that widget
inline int mouseYInWindow(const QWidget *window, const QMouseEvent *event)
{
    return window->mapFromGlobal(event->globalPosition().toPoint()).y();
}

}

#endif
