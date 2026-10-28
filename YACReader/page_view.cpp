#include "page_view.h"

#include <QPainter>

#include <optional>

void DoublePageNavigation::reset(int page)
{
    firstPages = { page };
    position = 0;
}

bool DoublePageNavigation::tracks(int page) const
{
    return position < firstPages.size() && firstPages.at(position) == page;
}

void DoublePageNavigation::follow(int page)
{
    if (!tracks(page)) {
        reset(page);
    }
}

bool DoublePageNavigation::showsTwoPages(int page, int numPages, const PagePairingFunction &pairingAt) const
{
    if (page + 1 >= numPages) {
        return false;
    }
    if (tracks(page) && position + 1 < firstPages.size() && page + 1 >= firstPages.at(position + 1)) {
        // Keep the second page out of the next view already recorded in history.
        return false;
    }
    return pairingAt(page) == PagePairing::Double;
}

int DoublePageNavigation::next(int page, int numPages, const PagePairingFunction &pairingAt)
{
    follow(page);
    if (position + 1 < firstPages.size()) {
        return firstPages.at(++position);
    }

    const int nextPage = page + (showsTwoPages(page, numPages, pairingAt) ? 2 : 1);
    if (nextPage >= numPages) {
        return page;
    }
    firstPages.append(nextPage);
    ++position;
    return nextPage;
}

int DoublePageNavigation::previous(int page, const PagePairingFunction &pairingAt)
{
    follow(page);
    if (position > 0) {
        return firstPages.at(--position);
    }

    // going back shows the two pages before the view when they make a double page
    const int pageStep = page >= 2 && pairingAt(page - 2) == PagePairing::Double ? 2 : 1;
    const int previousPage = qMax(page - pageStep, 0);
    if (previousPage != page) {
        firstPages.prepend(previousPage);
    }
    return previousPage;
}

QList<PageView> nearbyPageViews(int currentPage, int numPages, bool doublePageMode, const DoublePageNavigation &navigation,
                                const PagePairingFunction &pairingAt)
{
    QList<PageView> views;
    if (!doublePageMode) {
        for (const int page : { currentPage + 1, currentPage + 2, currentPage - 1 }) {
            if (page >= 0 && page < numPages) {
                views.append({ page, false });
            }
        }
        return views;
    }

    bool unknown = false;
    const PagePairingFunction knownPairingAt = [&](int page) {
        const PagePairing pairing = pairingAt(page);
        unknown = unknown || pairing == PagePairing::Unknown;
        return pairing;
    };
    // the view a page turn shows, or nothing when the turn or the view depends on a page that is not decoded yet
    const auto turn = [&](DoublePageNavigation &turned, int page, bool forward) -> std::optional<PageView> {
        unknown = false;
        const int nextPage = forward ? turned.next(page, numPages, knownPairingAt) : turned.previous(page, knownPairingAt);
        if (nextPage == page) {
            return std::nullopt;
        }
        const PageView view { nextPage, turned.showsTwoPages(nextPage, numPages, knownPairingAt) };
        if (unknown) {
            return std::nullopt;
        }
        return view;
    };

    DoublePageNavigation ahead = navigation;
    int page = currentPage;
    for (int turns = 0; turns < 2; ++turns) {
        const std::optional<PageView> next = turn(ahead, page, true);
        if (!next) {
            break;
        }
        views.append(*next);
        page = next->firstPage;
    }

    DoublePageNavigation behind = navigation;
    if (const std::optional<PageView> previous = turn(behind, currentPage, false)) {
        views.append(*previous);
    }

    return views;
}

DoublePageLayout doublePageLayout(QSize firstSize, QSize secondSize, int rotation)
{
    DoublePageLayout layout;
    const bool stacked = rotation == 90 || rotation == 270;
    if (stacked) {
        const int width = qMax(firstSize.width(), secondSize.width());
        firstSize.scale(width, firstSize.height(), Qt::KeepAspectRatioByExpanding);
        secondSize.scale(width, secondSize.height(), Qt::KeepAspectRatioByExpanding);
        layout.size = QSize(width, firstSize.height() + secondSize.height());
    } else {
        const int height = qMax(firstSize.height(), secondSize.height());
        firstSize.scale(firstSize.width(), height, Qt::KeepAspectRatioByExpanding);
        secondSize.scale(secondSize.width(), height, Qt::KeepAspectRatioByExpanding);
        layout.size = QSize(firstSize.width() + secondSize.width(), height);
    }

    // the first page comes second in comics turned upside down or to the left
    const bool firstPageSecond = rotation == 180 || rotation == 270;
    const QSize &leadingSize = firstPageSecond ? secondSize : firstSize;
    const QPoint seam = stacked ? QPoint(0, leadingSize.height()) : QPoint(leadingSize.width(), 0);
    layout.firstRect = QRect(firstPageSecond ? seam : QPoint(0, 0), firstSize);
    layout.secondRect = QRect(firstPageSecond ? QPoint(0, 0) : seam, secondSize);
    layout.seamRatio = stacked ? qreal(seam.y()) / layout.size.height() : qreal(seam.x()) / layout.size.width();
    layout.seamOrientation = stacked ? Qt::Horizontal : Qt::Vertical;
    return layout;
}

PageViewImages PageViewImages::of(int firstPage, const QImage &page, const QImage &nextPage, bool manga, int rotation)
{
    if (manga && !nextPage.isNull()) {
        return { firstPage, nextPage, page, rotation };
    }
    return { firstPage, page, nextPage, rotation };
}

PageViewKey PageViewImages::key() const
{
    return { firstPage, first.cacheKey(), second.cacheKey(), rotation };
}

DoublePageLayout PageViewImages::layout() const
{
    return doublePageLayout(first.size(), second.size(), rotation);
}

QSize PageViewImages::size() const
{
    return isDoublePage() ? layout().size : first.size();
}

QImage PageViewImages::image() const
{
    if (!isDoublePage()) {
        return first;
    }

    const DoublePageLayout pages = layout();
    const bool opaque = !first.hasAlphaChannel() && !second.hasAlphaChannel();
    QImage image(pages.size, opaque ? QImage::Format_RGB32 : QImage::Format_ARGB32_Premultiplied);
    if (!opaque) {
        image.fill(Qt::transparent);
    }
    {
        QPainter painter(&image);
        painter.drawImage(pages.firstRect, first);
        painter.drawImage(pages.secondRect, second);
    }
    return image;
}
