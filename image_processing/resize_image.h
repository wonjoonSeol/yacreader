#ifndef RESIZE_IMAGE_H
#define RESIZE_IMAGE_H

#include <QImage>
#include <QPixmap>

// The values are stored in settings and must not be renumbered. 3 is kept for upstream's Bicubic.
enum class ScaleMethod {
    Nearest = 0,
    Bilinear = 1,
    Lanczos4 = 2,
    Lanczos3 = 4,
    Lanczos2 = 5
};

bool isKnownScaleMethod(int value);

// Base scaling API - callers are responsible for supplying the correct target dimensions.

QPixmap scalePixmap(const QPixmap &pixmap, int width, int height, ScaleMethod method = ScaleMethod::Lanczos4);

QImage scaleImage(const QImage &image, int width, int height, ScaleMethod method = ScaleMethod::Lanczos4);

#endif // RESIZE_IMAGE_H
