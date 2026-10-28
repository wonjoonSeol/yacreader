#ifndef TOOLBAR_OVER_PAGE_H
#define TOOLBAR_OVER_PAGE_H

#include "mouse_handler.h"

namespace YACReader {

// Full screen hides the toolbar. Moving the mouse to the top of the screen shows it over the page until the mouse moves below it.
struct ToolBarOverPageState {
    bool shown;
    int mouseY; // in the full screen window, so 0 is the top of the screen
    int toolBarHeight;
    bool canReveal; // false while the mouse is busy with something else, like the magnifying glass
    bool mouseButtonPressed;
};

constexpr bool shouldShowToolBarOverPage(const ToolBarOverPageState &state)
{
    if (state.shown)
        return state.mouseY < state.toolBarHeight;

    return state.mouseY < mouseOverActivationHeight && state.canReveal && !state.mouseButtonPressed;
}

}

#endif
