#ifndef PAGES_INFORMATION_H
#define PAGES_INFORMATION_H

#include <QString>

namespace YACReader {

enum class PageSpread {
    None,
    LeftToRight,
    // manga
    RightToLeft
};

// page is 0-based: "10/100", or "10-11/100" for a double page spread, which reads "11-10/100" right to left
inline QString pagesInformation(int page, int numPages, PageSpread spread = PageSpread::None)
{
    QString pages = QString::number(page + 1);
    if (spread != PageSpread::None && page + 1 < numPages) {
        const QString nextPage = QString::number(page + 2);
        pages = spread == PageSpread::RightToLeft ? nextPage + "-" + pages : pages + "-" + nextPage;
    }
    return pages + "/" + QString::number(numPages);
}

}

#endif
