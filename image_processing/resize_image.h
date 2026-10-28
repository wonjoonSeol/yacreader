#ifndef RESIZE_IMAGE_H
#define RESIZE_IMAGE_H

#include <QImage>
#include <QPixmap>

#include <optional>

// The values are stored in settings and must not be renumbered. 3 is left free for Bicubic, which an upstream proposal stores as 3.
enum class ScaleMethod {
    Nearest = 0,
    Bilinear = 1,
    Lanczos = 2, // four lobes, labelled Lanczos4
    Lanczos3 = 4,
    Lanczos2 = 5
};

constexpr ScaleMethod defaultScaleMethod = ScaleMethod::Lanczos;

// The method stored as value, or nothing if no method is
std::optional<ScaleMethod> scaleMethodFromInt(int value);

// Base scaling API - callers are responsible for supplying the correct target dimensions.

QPixmap scalePixmap(const QPixmap &pixmap, int width, int height, ScaleMethod method = defaultScaleMethod);

QImage scaleImage(const QImage &image, int width, int height, ScaleMethod method = defaultScaleMethod);

#endif // RESIZE_IMAGE_H
