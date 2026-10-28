#include "acnet_upscale.h"

#include "AC/Core.hpp"
#include "AC/Util/Parallel.hpp"

#include <QThread>

#include <algorithm>
#include <cstring>
#include <mutex>

namespace {

// Source rows the network processes at once. Its two float scratch buffers take 64 bytes per
// pixel, so for a 1600 pixel wide page a strip needs about 28 MB instead of 245 MB for the page.
constexpr int stripRows = 256;

// Anime4KCPP's loops share one pool, which takes its size from ac::util::parallelForThreads
// when a loop first needs it, so the limit is set before anything else calls the library.
void limitThreads()
{
    static std::once_flag once;
    std::call_once(once, [] { ac::util::parallelForThreads = std::max(1, QThread::idealThreadCount() / 2); });
}

ac::core::model::ACNet<8> acnet(AcnetModel model)
{
    using Variant = ac::core::model::ACNet<8>::Variant;
    return { model == AcnetModel::F8B8Hdn ? Variant::B8_HDN : Variant::B4_NORMAL };
}

// The image's pixels without a copy. Writing through it skips QImage's detach, so only images
// created here, which nothing else shares, are written to.
ac::core::Image pixelsOf(const QImage &image)
{
    return { image.width(), image.height(), image.depth() / 8, ac::core::Image::UInt8, const_cast<uchar *>(image.constBits()), int(image.bytesPerLine()) };
}

// The library writes through the pointers of images it fails to allocate, so every image it
// writes to is allocated here and checked first. Its own scratch buffers throw instead (see
// third_party/anime4kcpp/README.md).
ac::core::Image allocate(int width, int height, int channels)
{
    return { width, height, channels, ac::core::Image::UInt8 };
}

// Doubles a one channel image in horizontal strips. An output row only depends on the source
// rows within one row per 3x3 convolution of it, so strips that overlap by that much match a
// single pass exactly. Returns false when cancelled or out of memory.
bool upscaleLuma(const ac::core::Image &src, ac::core::Image &dst, AcnetModel model, const std::atomic_bool &cancelled)
{
    const auto network = acnet(model);
    const auto processor = ac::core::Processor::create<ac::core::Processor::CPU>(0, network);
    const int convolutions = network.blocks() + 2;

    ac::core::Image strip = allocate(dst.width(), 2 * std::min(src.height(), stripRows + 2 * convolutions), 1);
    if (strip.empty()) {
        return false;
    }
    for (int top = 0; top < src.height(); top += stripRows) {
        if (cancelled) {
            return false;
        }
        const int rows = std::min(stripRows, src.height() - top);
        const int inTop = std::max(0, top - convolutions);
        const int inBottom = std::min(src.height(), top + rows + convolutions);
        ac::core::Image out = strip.view(0, 0, strip.width(), 2 * (inBottom - inTop));
        processor->process(src.view(0, inTop, src.width(), inBottom - inTop), out, 2.0);

        const int skippedRows = 2 * (top - inTop);
        for (int row = 0; row < 2 * rows; ++row) {
            std::memcpy(dst.line(2 * top + row), out.line(skippedRows + row), dst.width());
        }
    }
    return true;
}

QImage upscaleGray(const QImage &image, AcnetModel model, const std::atomic_bool &cancelled)
{
    const QImage src = image.convertToFormat(QImage::Format_Grayscale8);
    QImage dst(2 * image.width(), 2 * image.height(), QImage::Format_Grayscale8);
    if (src.isNull() || dst.isNull()) {
        return { };
    }

    ac::core::Image out = pixelsOf(dst);
    return upscaleLuma(pixelsOf(src), out, model, cancelled) ? dst : QImage();
}

// ACNet's colour conversions read and write bytes in R, G, B, A order, hence the RGBA formats.
QImage upscaleColor(const QImage &image, AcnetModel model, const std::atomic_bool &cancelled)
{
    const bool hasAlpha = image.hasAlphaChannel();
    const QImage::Format format = hasAlpha ? QImage::Format_RGBA8888 : QImage::Format_RGBX8888;
    const int width = image.width(), height = image.height();

    ac::core::Image luma = allocate(width, height, 1);
    ac::core::Image chroma = allocate(width, height, hasAlpha ? 3 : 2);
    {
        const QImage src = image.convertToFormat(format);
        if (src.isNull() || luma.empty() || chroma.empty()) {
            return { };
        }
        if (hasAlpha) {
            ac::core::rgba2yuva(pixelsOf(src), luma, chroma);
        } else {
            ac::core::rgb2yuv(pixelsOf(src), luma, chroma);
        }
    }

    ac::core::Image upscaledLuma = allocate(2 * width, 2 * height, 1);
    if (upscaledLuma.empty() || !upscaleLuma(luma, upscaledLuma, model, cancelled)) {
        return { };
    }
    luma = { };
    ac::core::Image upscaledChroma = allocate(2 * width, 2 * height, chroma.channels());
    if (upscaledChroma.empty()) {
        return { };
    }
    ac::core::resize(chroma, upscaledChroma, 2.0, 2.0);
    chroma = { };

    QImage dst(2 * width, 2 * height, format);
    if (dst.isNull() || cancelled) {
        return { };
    }

    ac::core::Image out = pixelsOf(dst);
    if (hasAlpha) {
        ac::core::yuva2rgba(upscaledLuma, upscaledChroma, out);
    } else {
        ac::core::yuv2rgb(upscaledLuma, upscaledChroma, out);
    }

    dst.convertTo(hasAlpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    return dst;
}

bool isGray(const QImage &image)
{
    switch (image.format()) {
    case QImage::Format_Grayscale8:
    case QImage::Format_Grayscale16:
        return true;
    case QImage::Format_Indexed8:
    case QImage::Format_Mono:
    case QImage::Format_MonoLSB:
        return image.isGrayscale() && !image.hasAlphaChannel();
    default:
        return false;
    }
}

// Also keeps every buffer below 2 GB, where the library's int sizes would overflow
bool withinPixelLimit(QSize size)
{
    return qint64(size.width()) * size.height() <= kAcnetMaxPixels;
}

// Upscales run on pool threads, where an exception would end the program: anything the
// library throws, such as std::bad_alloc, gives a null image instead.
QImage upscale2x(const QImage &image, AcnetModel model, const std::atomic_bool &cancelled)
{
    if (image.isNull() || !withinPixelLimit(image.size())) {
        return { };
    }

    limitThreads();
    try {
        return isGray(image) ? upscaleGray(image, model, cancelled) : upscaleColor(image, model, cancelled);
    } catch (...) {
        return { };
    }
}

}

bool acnetUpscales(QSize imageSize, QSize targetSize)
{
    if (imageSize.isEmpty() || targetSize.isEmpty() || !withinPixelLimit(imageSize)) {
        return false;
    }

    const double enlargement = std::max(double(targetSize.width()) / imageSize.width(), double(targetSize.height()) / imageSize.height());
    return enlargement >= kAcnetMinEnlargement;
}

QImage upscale2xAcnet(const QImage &image, AcnetModel model)
{
    const std::atomic_bool notCancelled = false;
    return upscale2x(image, model, notCancelled);
}

QImage scaleImageWithAcnet(const QImage &image, QSize size, ScaleMethod method, AcnetModel model, const std::atomic_bool &cancelled)
{
    if (!acnetUpscales(image.size(), size)) {
        return { };
    }

    const QImage doubled = upscale2x(image, model, cancelled);
    if (doubled.isNull() || cancelled) {
        return { };
    }

    // Lanczos for the shrink: the selected method may be Nearest, which aliases the fine lines
    // ACNet just sharpened. Past 2x it is an enlargement again, so it is the user's choice.
    const bool shrinks = size.width() <= doubled.width() && size.height() <= doubled.height();
    try {
        return scaleImage(doubled, size.width(), size.height(), shrinks ? ScaleMethod::Lanczos : method);
    } catch (...) {
        return { };
    }
}
