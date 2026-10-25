#include "page_view.h"

#include <QPainter>

#include <optional>

namespace {

// A view replayed from the history ends where the next recorded view starts
std::optional<PageView> viewAt(int page, int numPages, int nextRecordedPage, const std::function<PagePairing(int)> &pairingAt)
{
    if (page + 1 >= numPages || (nextRecordedPage >= 0 && page + 1 >= nextRecordedPage)) {
        return PageView { page, false };
    }

    switch (pairingAt(page)) {
    case PagePairing::Single:
        return PageView { page, false };
    case PagePairing::Double:
        return PageView { page, true };
    case PagePairing::Unknown:
        break;
    }
    return std::nullopt;
}

}

QList<PageView> nearbyPageViews(PageView current, int numPages, bool doublePageMode, const PageViewHistory &history,
                                const std::function<PagePairing(int)> &pairingAt)
{
    QList<PageView> views;
    if (!doublePageMode) {
        for (const int page : { current.firstPage + 1, current.firstPage + 2, current.firstPage - 1 }) {
            if (page >= 0 && page < numPages) {
                views.append({ page, false });
            }
        }
        return views;
    }

    // Render starts a new history when the current view is not the one it recorded last
    const bool historyTracksCurrent = history.position < history.firstPages.size() && history.firstPages.at(history.position) == current.firstPage;
    const QList<int> recorded = historyTracksCurrent ? history.firstPages : QList<int> { current.firstPage };
    const int position = historyTracksCurrent ? history.position : 0;
    const auto recordedAt = [&recorded](int index) {
        return index >= 0 && index < recorded.size() ? recorded.at(index) : -1;
    };

    PageView view = current;
    for (int turn = 1; turn <= 2; ++turn) {
        int nextPage = recordedAt(position + turn);
        if (nextPage < 0) {
            nextPage = view.firstPage + (view.doublePage ? 2 : 1);
        }
        if (nextPage >= numPages) {
            break;
        }

        const std::optional<PageView> next = viewAt(nextPage, numPages, recordedAt(position + turn + 1), pairingAt);
        if (!next) {
            break;
        }
        views.append(*next);
        view = *next;
    }

    int previousPage = recordedAt(position - 1);
    if (previousPage < 0) {
        // going back shows the two pages before the current view when they make a double page
        switch (current.firstPage >= 2 ? pairingAt(current.firstPage - 2) : PagePairing::Single) {
        case PagePairing::Single:
            previousPage = current.firstPage - 1;
            break;
        case PagePairing::Double:
            previousPage = current.firstPage - 2;
            break;
        case PagePairing::Unknown:
            break;
        }
    }
    if (previousPage >= 0) {
        if (const std::optional<PageView> previous = viewAt(previousPage, numPages, current.firstPage, pairingAt)) {
            views.append(*previous);
        }
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

PageViewKey PageViewImages::key() const
{
    return { firstPage, first.cacheKey(), second.cacheKey() };
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
