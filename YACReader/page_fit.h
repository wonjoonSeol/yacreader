#ifndef PAGE_FIT_H
#define PAGE_FIT_H

#include <QSize>

namespace YACReader {

enum FitMode {
    ToWidth = 0x01,
    ToHeight = 0x02,
    FullRes = 0x03,
    FullPage = 0x04
};

}

struct PageFitOptions {
    QSize viewSize;
    YACReader::FitMode fitMode = YACReader::FitMode::FullPage;
    bool enlargeImages = true;
    int zoom = 100;
    qreal devicePixelRatio = 1.0;
};

struct PageFit {
    // of the widget that shows the page
    QSize size;
    // of the scaled image, in device pixels
    QSize pixelSize;
};

PageFit fitPage(QSize pageSize, const PageFitOptions &options);

#endif // PAGE_FIT_H
