#ifndef ACNET_UPSCALE_H
#define ACNET_UPSCALE_H

#include <QImage>

// Anime4KCPP's ACNet models: F<features>B<blocks>, HDN for the stronger denoising variant.
// F8B4 is the default: on line art it gains most of what F8B8-HDN gains over Lanczos
// (29.5 against 29.9 dB, Lanczos 25.8) at about 60% of its time.
enum class AcnetModel {
    F8B4,
    F8B8Hdn
};

// Doubles both sides of an image with ACNet on the CPU. The network sharpens luma; chroma
// and alpha are scaled with Catmull-Rom. Returns Grayscale8 for grayscale images, RGB32 for
// opaque ones and ARGB32 for the rest, or a null image if the input is null.
QImage upscale2xAcnet(const QImage &image, AcnetModel model = AcnetModel::F8B4);

// The most threads one upscale uses. Defaults to half the CPU's threads, like the reader's
// decode and scale pools.
void setAcnetMaxThreads(int threads);

#endif // ACNET_UPSCALE_H
