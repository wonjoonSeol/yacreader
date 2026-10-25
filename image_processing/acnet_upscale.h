#ifndef ACNET_UPSCALE_H
#define ACNET_UPSCALE_H

#include "resize_image.h"

#include <QImage>
#include <QSize>

// Anime4KCPP's ACNet models: F<features>B<blocks>, HDN for the stronger denoising variant.
// F8B4 is the default: on line art it gains most of what F8B8-HDN gains over Lanczos
// (29.5 against 29.9 dB, Lanczos 25.8) at about 60% of its time. The values are stored in
// the settings.
enum class AcnetModel {
    F8B4 = 0,
    F8B8Hdn = 1
};

// Doubles both sides of an image with ACNet on the CPU. The network sharpens luma; chroma
// and alpha are scaled with Catmull-Rom. Returns Grayscale8 for grayscale images, RGB32 for
// opaque ones and ARGB32 for the rest, or a null image if the input is null.
QImage upscale2xAcnet(const QImage &image, AcnetModel model = AcnetModel::F8B4);

// Scales an image to `size` through ACNet when that enlarges it on either side: ACNet doubles
// it, then `method` scales the result to `size`. Returns a null image when the image is not
// enlarged.
QImage scaleImageWithAcnet(const QImage &image, QSize size, ScaleMethod method, AcnetModel model = AcnetModel::F8B4);

// The most threads the upscales use, together. Defaults to half the CPU's threads, like the
// reader's decode and scale pools. Only a limit set before the first upscale can raise the
// total, since the threads are created then.
void setAcnetMaxThreads(int threads);

#endif // ACNET_UPSCALE_H
