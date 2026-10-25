#include "page_fit.h"

#include <cmath>

PageFit fitPage(QSize pageSize, const PageFitOptions &options)
{
    QSize size = pageSize;
    switch (options.fitMode) {
    case YACReader::FitMode::FullRes:
        break;
    case YACReader::FitMode::ToWidth:
        if (!options.enlargeImages && options.viewSize.width() > size.width()) {
            break;
        }
        size.scale(options.viewSize.width(), 0, Qt::KeepAspectRatioByExpanding);
        break;
    case YACReader::FitMode::ToHeight:
        if (!options.enlargeImages && options.viewSize.height() > size.height()) {
            break;
        }
        size.scale(0, options.viewSize.height(), Qt::KeepAspectRatioByExpanding);
        break;
        // if everything fails showing the full page is a good idea
    case YACReader::FitMode::FullPage:
    default:
        size.scale(options.viewSize, Qt::KeepAspectRatio);
        break;
    }

    if (options.zoom != 100) {
        size.scale(static_cast<int>(std::floor(size.width() * options.zoom / 100.0f)), 0, Qt::KeepAspectRatioByExpanding);
    }

    const QSize pixelSize(qRound(size.width() * options.devicePixelRatio), qRound(size.height() * options.devicePixelRatio));
    return { size, pixelSize };
}
