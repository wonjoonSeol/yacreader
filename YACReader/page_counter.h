#ifndef PAGE_COUNTER_H
#define PAGE_COUNTER_H

#include <QList>
#include <QRect>
#include <QString>

#include <algorithm>

namespace YACReader {

// page is 0-based: "10/100", or "10-11/100" for a double page spread, which manga mode lists right to left as "11-10/100"
inline QString pagesInformation(int page, int numPages, bool spread = false, bool manga = false)
{
    QString pages = QString::number(page + 1);
    if (spread && page + 1 < numPages) {
        const QString nextPage = QString::number(page + 2);
        pages = manga ? nextPage + "-" + pages : pages + "-" + nextPage;
    }
    return pages + "/" + QString::number(numPages);
}

// The page counter goes in the top left corner of the page, below the toolbars laid over that corner, such as the toolbar shown over the page in full screen.
// The toolbar areas are in page coordinates.
inline int pageCounterTop(const QList<QRect> &toolBarAreas)
{
    int top = 0;
    for (const QRect &area : toolBarAreas) {
        if (area.contains(0, 0))
            top = std::max(top, area.bottom() + 1);
    }
    return top;
}

}

#endif
