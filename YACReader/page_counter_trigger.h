#ifndef PAGE_COUNTER_TRIGGER_H
#define PAGE_COUNTER_TRIGGER_H

#include <optional>

// The page counter shows when the page on screen changes. In continuous scroll that is the scroll's
// reading page: Render only catches up with the scroll before its own page turns, so its page can lag
// behind, or already be the page a Go to page scrolls to.
class PageCounterTrigger
{
public:
    // The first page shown after this counts as a page turn, as when a comic opens
    void reset() { shownPage = -1; }

    // Called once the view is on its new page. continuousReadingPage is set in continuous scroll.
    // Returns whether the page on screen differs from the one at the last call.
    bool pageTurned(int renderPage, std::optional<int> continuousReadingPage)
    {
        const int page = continuousReadingPage.value_or(renderPage);
        const bool turned = page != shownPage;
        shownPage = page;
        return turned;
    }

private:
    int shownPage = -1;
};

#endif
