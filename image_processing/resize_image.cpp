#include "resize_image.h"

#include "bicubic_scale.h"
#include "lancir.h"

#include <QVariant>

// Lanczos and Bicubic read 4 bytes per pixel as ARGB32. RGB32 has the same
// layout with opaque alpha, so it is read as it is.
static QImage argb32(const QImage &image)
{
    if (image.format() == QImage::Format_ARGB32 || image.format() == QImage::Format_RGB32)
        return image;
    return image.convertToFormat(QImage::Format_ARGB32);
}

static QImage scaleImageLancir(const QImage &src, int width, int height, double lobes)
{
    QImage dst(width, height, QImage::Format_ARGB32);
    if (dst.isNull())
        return dst;

    // SrcSSize / NewSSize are in elements; for uint8_t that equals bytes,
    // so bytesPerLine() covers any Qt row-alignment padding correctly.
    avir::CLancIRParams params(src.bytesPerLine(), dst.bytesPerLine());
    params.la = lobes;

    avir::CLancIR lancir;
    lancir.resizeImage<uint8_t, uint8_t>(
            src.constBits(), src.width(), src.height(),
            dst.bits(), width, height, 4, &params);

    return dst;
}

ScaleMethod scaleMethodFromSetting(const QVariant &value)
{
    bool ok = false;
    const auto method = static_cast<ScaleMethod>(value.toInt(&ok));
    if (!ok)
        return defaultScaleMethod;

    switch (method) {
    case ScaleMethod::Nearest:
    case ScaleMethod::Bilinear:
    case ScaleMethod::Lanczos4:
    case ScaleMethod::Bicubic:
    case ScaleMethod::Lanczos3:
    case ScaleMethod::Lanczos2:
        return method;
    }
    return defaultScaleMethod;
}

// ---- QPixmap API ------------------------------------------------------------

QPixmap scalePixmap(const QPixmap &pixmap, int width, int height, ScaleMethod method)
{
    if ((pixmap.width() == width && pixmap.height() == height) || pixmap.isNull())
        return pixmap;

    switch (method) {
    case ScaleMethod::Nearest:
        return pixmap.scaled(width, height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    case ScaleMethod::Bilinear:
        return pixmap.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    case ScaleMethod::Lanczos4:
    case ScaleMethod::Bicubic:
    case ScaleMethod::Lanczos3:
    case ScaleMethod::Lanczos2:
        return QPixmap::fromImage(scaleImage(pixmap.toImage(), width, height, method));
    }
    return pixmap;
}

// ---- QImage API (avoids QPixmap round-trip in ContinuousPageWidget) ---------

QImage scaleImage(const QImage &image, int width, int height, ScaleMethod method)
{
    if ((image.width() == width && image.height() == height) || image.isNull())
        return image;
    if (width <= 0 || height <= 0)
        return QImage();

    switch (method) {
    case ScaleMethod::Nearest:
        return image.scaled(width, height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    case ScaleMethod::Bilinear:
        return image.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    case ScaleMethod::Lanczos4:
        return scaleImageLancir(argb32(image), width, height, 4.0);
    case ScaleMethod::Bicubic:
        return scaleImageBicubic(argb32(image), width, height);
    case ScaleMethod::Lanczos3:
        return scaleImageLancir(argb32(image), width, height, 3.0);
    case ScaleMethod::Lanczos2:
        return scaleImageLancir(argb32(image), width, height, 2.0);
    }
    return image;
}
