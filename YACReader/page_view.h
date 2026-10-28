#ifndef PAGE_VIEW_H
#define PAGE_VIEW_H

#include <QHashFunctions>
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
};

// Whether a page and the next one are shown together in double page mode by their shape: both portrait, and not the
// cover when it is shown alone. The recorded history can still keep them apart, see DoublePageNavigation.
enum class PagePairing {
    Single,
    Double,
    // one of the two pages is not decoded yet, so Render shows the first one alone for now
    Unknown
};

using PagePairingFunction = std::function<PagePairing(int page)>;

// Double page mode's page turns, as Render makes them. The first pages of the views visited are recorded and page
// turns replay them, so going back and forth keeps the same page groupings. A pairing that is not known yet counts as
// single, like Render shows it.
class DoublePageNavigation
{
public:
    // Starts a new history at page
    void reset(int page);
    // Whether the view at page shows page + 1 too: not when the next recorded view starts at page + 1
    bool showsTwoPages(int page, int numPages, const PagePairingFunction &pairingAt) const;
    // The first page of the view after the one at page, or page itself at the last view
    int next(int page, int numPages, const PagePairingFunction &pairingAt);
    // The first page of the view before the one at page, or page itself at the first view
    int previous(int page, const PagePairingFunction &pairingAt);

private:
    bool tracks(int page) const;
    // a page turn from a view the history did not record starts a new history
    void follow(int page);

    QList<int> firstPages { 0 };
    int position = 0;
};

// The views the next two page turns and the previous one would show, in that order. In double page mode they are
// turned with a copy of navigation. A view that depends on a page that is not decoded yet is left out, and so are
// the views after it.
QList<PageView> nearbyPageViews(int currentPage, int numPages, bool doublePageMode, const DoublePageNavigation &navigation,
                                const PagePairingFunction &pairingAt);

// Where the two pages of a double page go. Both pages get the height of the taller one, or, rotated by 90 or 270
// degrees, the width of the wider one, and they are stacked.
struct DoublePageLayout {
    QSize size;
    QRect firstRect;
    QRect secondRect;
    qreal seamRatio = 0.5;
    Qt::Orientation seamOrientation = Qt::Vertical;
};

DoublePageLayout doublePageLayout(QSize firstSize, QSize secondSize, int rotation);

// Identifies the decoded images a view shows and how they are laid out
struct PageViewKey {
    int firstPage = -1;
    qint64 firstImageKey = 0;
    qint64 secondImageKey = 0;
    int rotation = 0;

    bool operator==(const PageViewKey &other) const = default;
};

inline size_t qHash(const PageViewKey &key, size_t seed = 0)
{
    return qHashMulti(seed, key.firstPage, key.firstImageKey, key.secondImageKey, key.rotation);
}

// The decoded pages of a view. The two pages of a double page are in the order they are laid out, so manga mode
// swaps them.
struct PageViewImages {
    int firstPage = -1;
    QImage first;
    QImage second;
    int rotation = 0;

    // nextPage is null for a single page. Manga mode lays the next page out first.
    static PageViewImages of(int firstPage, const QImage &page, const QImage &nextPage, bool manga, int rotation);

    bool isDoublePage() const { return !second.isNull(); }
    PageViewKey key() const;
    DoublePageLayout layout() const;
    QSize size() const;
    // the page at full resolution, or both pages drawn together
    QImage image() const;
};

#endif // PAGE_VIEW_H
