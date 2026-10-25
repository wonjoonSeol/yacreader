#ifndef PAGE_VIEW_H
#define PAGE_VIEW_H

#include <QImage>
#include <QList>
#include <QRect>
#include <QSize>

#include <functional>

// What the reader shows at once in single or double page mode
struct PageView {
    int firstPage = -1;
    // shows firstPage + 1 next to it
    bool doublePage = false;

    bool operator==(const PageView &other) const = default;
};

// Whether double page mode shows a page together with the next one
enum class PagePairing {
    Single,
    Double,
    // one of the two pages is not decoded yet
    Unknown
};

// The first pages of the double page views visited, which page turns replay so the groupings stay the same
struct PageViewHistory {
    QList<int> firstPages;
    int position = 0;
};

// The views the next two page turns and the previous one would show, in that order, following Render's rules.
// Views that depend on a page that is not decoded yet are left out.
QList<PageView> nearbyPageViews(PageView current, int numPages, bool doublePageMode, const PageViewHistory &history,
                                const std::function<PagePairing(int)> &pairingAt);

// Both pages get the height of the taller one, or the width of the wider one when rotated by 90 or 270 degrees
struct DoublePageLayout {
    QSize size;
    QRect firstRect;
    QRect secondRect;
    qreal seamRatio = 0.5;
    Qt::Orientation seamOrientation = Qt::Vertical;
};

DoublePageLayout doublePageLayout(QSize firstSize, QSize secondSize, int rotation);

// Identifies the decoded images a view shows
struct PageViewKey {
    int firstPage = -1;
    qint64 firstImageKey = 0;
    qint64 secondImageKey = 0;

    bool operator==(const PageViewKey &other) const = default;
};

// The decoded pages of a view. The two pages of a double page are in the order they are laid out, so manga mode
// swaps them.
struct PageViewImages {
    int firstPage = -1;
    QImage first;
    QImage second;
    int rotation = 0;

    bool isDoublePage() const { return !second.isNull(); }
    PageViewKey key() const;
    DoublePageLayout layout() const;
    QSize size() const;
    // the page at full resolution, or both pages drawn together
    QImage image() const;
};

#endif // PAGE_VIEW_H
