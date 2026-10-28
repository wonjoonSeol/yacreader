#ifndef ACNET_UPSCALE_H
#define ACNET_UPSCALE_H

#include "resize_image.h"

#include <QImage>
#include <QSize>

#include <atomic>

// Anime4KCPP's ACNet models: F<features>B<blocks>, HDN for the stronger denoising variant.
// F8B4 is the default: on line art it gains most of what F8B8-HDN gains over Lanczos
// (29.5 against 29.9 dB, Lanczos 25.8) at about 60% of its time. The values are stored in
// the settings.
enum class AcnetModel {
    F8B4 = 0,
    F8B8Hdn = 1
};

// ACNet only runs when a page is shown at least this much larger than its size, on either
// side. Every upscale is a full 2x pass whatever the target, so small enlargements are left
// to the scaling method.
constexpr double kAcnetMinEnlargement = 1.5;
// ... and only on pages of at most this many pixels. An upscale peaks at about 41 bytes per
// page pixel (157 MB measured for a 1600 x 2400 colour page), so this keeps one near 165 MB;
// bigger pages rarely need enlarging, and long webtoon strips stay out. Re-measure on real
// comics before changing either limit.
constexpr qint64 kAcnetMaxPixels = 4'000'000;

// Whether scaleImageWithAcnet upscales an image of `imageSize` shown at `targetSize`.
bool acnetUpscales(QSize imageSize, QSize targetSize);

// Doubles both sides of an image with ACNet on the CPU. The network sharpens luma; chroma
// and alpha are scaled with Catmull-Rom. Returns Grayscale8 for grayscale images, RGB32 for
// opaque ones and ARGB32 for the rest. Returns a null image if the input is null, larger
// than kAcnetMaxPixels, or memory runs out.
//
// The network runs on a pool of half the CPU's threads (one thread on CPUs with three or
// fewer, without a pool), which upscales running at the same time share.
QImage upscale2xAcnet(const QImage &image, AcnetModel model = AcnetModel::F8B4);

// Scales an image to `size` through ACNet when acnetUpscales() allows it: ACNet doubles it,
// then Lanczos takes it down to `size`, or `method` up when `size` is more than double.
// Returns a null image when it does not upscale, and when `cancelled` is set while it runs.
QImage scaleImageWithAcnet(const QImage &image, QSize size, ScaleMethod method, AcnetModel model,
                           const std::atomic_bool &cancelled);

#endif // ACNET_UPSCALE_H
