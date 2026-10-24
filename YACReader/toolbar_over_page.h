#ifndef TOOLBAR_OVER_PAGE_H
#define TOOLBAR_OVER_PAGE_H

namespace YACReader {

// Full screen hides the toolbar. Moving the mouse to the top of the screen shows it over the page until the mouse leaves it.
struct ToolBarOverPageState {
    bool shown;
    int mouseY; // in the full screen window, so 0 is the top of the screen
    int toolBarHeight;
    bool magnifyingGlassShown;
    bool mouseButtonPressed;
};

// as tall as the band at the bottom of the screen that shows the go to flow
constexpr int toolBarOverPageActivationHeight = 15;

constexpr bool showsToolBarOverPage(const ToolBarOverPageState &state)
{
    if (state.shown)
        return state.mouseY < state.toolBarHeight;

    return state.mouseY < toolBarOverPageActivationHeight && !state.magnifyingGlassShown && !state.mouseButtonPressed;
}

}

#endif
