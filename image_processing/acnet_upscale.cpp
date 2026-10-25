#include "acnet_upscale.h"

#include "AC/Core.hpp"
#include "AC/Util/Parallel.hpp"

#include <QThread>

#include <algorithm>
#include <atomic>
#include <cstring>

namespace {

// Source rows the network processes at once. Its two float scratch buffers take 64 bytes per
// pixel, so for a 1600 pixel wide page a strip needs about 28 MB instead of 245 MB for the page.
constexpr int stripRows = 256;

std::atomic<int> maxThreads { 0 };

int threadLimit()
{
    const int threads = maxThreads;
    return threads > 0 ? threads : std::max(1, QThread::idealThreadCount() / 2);
}

ac::core::model::ACNet<8> acnet(AcnetModel model)
{
    using Variant = ac::core::model::ACNet<8>::Variant;
    return { model == AcnetModel::F8B8Hdn ? Variant::B8_HDN : Variant::B4_NORMAL };
}

ac::core::Image pixelsOf(QImage &image)
{
    return { image.width(), image.height(), image.depth() / 8, ac::core::Image::UInt8, image.bits(), int(image.bytesPerLine()) };
}

ac::core::Image pixelsOf(const QImage &image)
{
    return { image.width(), image.height(), image.depth() / 8, ac::core::Image::UInt8, const_cast<uchar *>(image.constBits()), int(image.bytesPerLine()) };
}

// Doubles a one channel image in horizontal strips. An output row only depends on the source
// rows within one row per 3x3 convolution of it, so strips that overlap by that much match a
// single pass exactly.
void upscaleLuma(const ac::core::Image &src, ac::core::Image &dst, AcnetModel model)
{
    ac::util::parallelForThreads = threadLimit();
    const auto network = acnet(model);
    const auto processor = ac::core::Processor::create<ac::core::Processor::CPU>(0, network);
    const int convolutions = network.blocks() + 2;

    ac::core::Image strip(dst.width(), 2 * std::min(src.height(), stripRows + 2 * convolutions), 1, ac::core::Image::UInt8);
    for (int top = 0; top < src.height(); top += stripRows) {
        const int rows = std::min(stripRows, src.height() - top);
        const int inTop = std::max(0, top - convolutions);
        const int inBottom = std::min(src.height(), top + rows + convolutions);
        ac::core::Image out = strip.view(0, 0, strip.width(), 2 * (inBottom - inTop));
        processor->process(src.view(0, inTop, src.width(), inBottom - inTop), out, 2.0);

        const int skippedRows = 2 * (top - inTop);
        for (int row = 0; row < 2 * rows; ++row)
            std::memcpy(dst.line(2 * top + row), out.line(skippedRows + row), dst.width());
    }
}

QImage upscaleGray(const QImage &image, AcnetModel model)
{
    const QImage src = image.convertToFormat(QImage::Format_Grayscale8);
    QImage dst(2 * src.width(), 2 * src.height(), QImage::Format_Grayscale8);
    if (dst.isNull())
        return { };

    ac::core::Image out = pixelsOf(dst);
    upscaleLuma(pixelsOf(src), out, model);
    return dst;
}

// ACNet's colour conversions read and write bytes in R, G, B, A order, hence the RGBA formats.
QImage upscaleColor(const QImage &image, AcnetModel model)
{
    const bool hasAlpha = image.hasAlphaChannel();
    const QImage::Format format = hasAlpha ? QImage::Format_RGBA8888 : QImage::Format_RGBX8888;

    ac::core::Image luma, chroma;
    {
        const QImage src = image.convertToFormat(format);
        if (hasAlpha)
            ac::core::rgba2yuva(pixelsOf(src), luma, chroma);
        else
            ac::core::rgb2yuv(pixelsOf(src), luma, chroma);
    }

    ac::core::Image upscaledLuma(2 * luma.width(), 2 * luma.height(), 1, ac::core::Image::UInt8);
    upscaleLuma(luma, upscaledLuma, model);
    luma = { };
    const ac::core::Image upscaledChroma = ac::core::resize(chroma, 2.0, 2.0);
    chroma = { };

    QImage dst(upscaledLuma.width(), upscaledLuma.height(), format);
    if (dst.isNull())
        return { };

    ac::core::Image out = pixelsOf(dst);
    if (hasAlpha)
        ac::core::yuva2rgba(upscaledLuma, upscaledChroma, out);
    else
        ac::core::yuv2rgb(upscaledLuma, upscaledChroma, out);

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

}

QImage upscale2xAcnet(const QImage &image, AcnetModel model)
{
    if (image.isNull())
        return { };

    return isGray(image) ? upscaleGray(image, model) : upscaleColor(image, model);
}

void setAcnetMaxThreads(int threads)
{
    maxThreads = threads;
}
