#include "resize_image.h"

#include "lancir.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

static QImage scaleImageLancir(const QImage &image, int width, int height, double lobes)
{
    QImage src = (image.format() == QImage::Format_ARGB32)
            ? image
            : image.convertToFormat(QImage::Format_ARGB32);

    QImage dst(width, height, QImage::Format_ARGB32);

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

// ---- Bicubic (Catmull-Rom, separable two-pass resampler) ---------------------

struct BicubicTap {
    int index;
    int weight; // 16.16 fixed point, taps sum to 1<<16
};

// Catmull-Rom kernel (B=0, C=1/2): the classic "bicubic" of image editors.
static inline double bicubicKernel(double t)
{
    t = std::fabs(t);
    if (t <= 1.0)
        return 1.5 * t * t * t - 2.5 * t * t + 1.0;
    if (t < 2.0)
        return -0.5 * t * t * t + 2.5 * t * t - 4.0 * t + 2.0;
    return 0.0;
}

// Per-destination-tap weights. When downscaling (scale > 1) the kernel is
// widened by the scale factor so each output pixel integrates the source area
// it covers, which keeps aliasing/moiré under control.
static std::vector<std::vector<BicubicTap>> bicubicContributions(int srcSize, int dstSize)
{
    std::vector<std::vector<BicubicTap>> contributions(dstSize);
    const double scale = double(srcSize) / double(dstSize);
    const double filterScale = scale > 1.0 ? scale : 1.0;

    for (int i = 0; i < dstSize; ++i) {
        const double center = (i + 0.5) * scale - 0.5;
        const int left = std::max(0, int(std::ceil(center - 2.0 * filterScale)));
        const int right = std::min(srcSize - 1, int(std::floor(center + 2.0 * filterScale)));

        std::vector<double> raw;
        raw.reserve(8);
        double sum = 0.0;
        for (int j = left; j <= right; ++j) {
            const double w = bicubicKernel((j - center) / filterScale);
            if (w == 0.0)
                continue;
            raw.push_back(w);
            sum += w;
        }
        size_t k = 0;
        for (int j = left; j <= right && k < raw.size(); ++j) {
            const double w = bicubicKernel((j - center) / filterScale);
            if (w == 0.0)
                continue;
            contributions[i].push_back({ j, int(std::llround(raw[k] / sum * 65536.0)) });
            ++k;
        }
    }
    return contributions;
}

// Rounds a horizontal-pass sum (16.16 weights x 8-bit samples) to the signed
// 8.6 intermediate. Catmull-Rom's overshoot has to reach the vertical pass
// unclipped, or the negative lobes cannot cancel out there. The sums stay
// within about -32 to 287 grey levels, so the clamp only guards the cast.
static inline int16_t toIntermediate(int sum)
{
    return int16_t(std::clamp((sum + (1 << 9)) >> 10, -(511 << 6), 511 << 6));
}

// Rounds a vertical-pass sum (16.16 weights x 8.6 samples, so 22 fraction
// bits) to an 8-bit sample.
static inline uint8_t toSample(int64_t sum)
{
    return uint8_t(std::clamp(int((sum + (1 << 21)) >> 22), 0, 255));
}

// Averages the stored sRGB values, like the other methods. Linear light would
// make shrunk line art and screentones lighter than with any other method, and
// it deepens the dark halo beside enlarged edges.
static QImage scaleImageBicubic(const QImage &image, int width, int height)
{
    QImage src = (image.format() == QImage::Format_ARGB32)
            ? image
            : image.convertToFormat(QImage::Format_ARGB32);

    const int sw = src.width();
    const int sh = src.height();

    // Horizontal pass into a signed BGRA buffer at 8.6 fixed point.
    std::vector<int16_t> tmp(size_t(width) * sh * 4);
    const auto hWeights = bicubicContributions(sw, width);
    const auto hPass = [&](int yBegin, int yEnd) {
        for (int y = yBegin; y < yEnd; ++y) {
            const QRgb *row = reinterpret_cast<const QRgb *>(src.constScanLine(y));
            int16_t *outRow = &tmp[size_t(y) * width * 4];
            for (int x = 0; x < width; ++x) {
                int32_t b = 0, g = 0, r = 0, a = 0;
                for (const auto &tap : hWeights[x]) {
                    const QRgb px = row[tap.index];
                    b += tap.weight * qBlue(px);
                    g += tap.weight * qGreen(px);
                    r += tap.weight * qRed(px);
                    a += tap.weight * qAlpha(px);
                }
                int16_t *out = &outRow[size_t(x) * 4];
                out[0] = toIntermediate(b);
                out[1] = toIntermediate(g);
                out[2] = toIntermediate(r);
                out[3] = toIntermediate(a);
            }
        }
    };

    // Vertical pass into the destination image.
    QImage dst(width, height, QImage::Format_ARGB32);
    // QImage::scanLine() detaches the image, which is not safe to call from
    // several threads at once, so take the pixels once before splitting.
    uchar *dstBits = dst.bits();
    const qsizetype dstBytesPerLine = dst.bytesPerLine();
    const auto vWeights = bicubicContributions(sh, height);
    const auto vPass = [&](int yBegin, int yEnd) {
        for (int y = yBegin; y < yEnd; ++y) {
            QRgb *row = reinterpret_cast<QRgb *>(dstBits + y * dstBytesPerLine);
            for (int x = 0; x < width; ++x) {
                int64_t b = 0, g = 0, r = 0, a = 0;
                for (const auto &tap : vWeights[y]) {
                    const int16_t *in = &tmp[(size_t(tap.index) * width + x) * 4];
                    b += int64_t(tap.weight) * in[0];
                    g += int64_t(tap.weight) * in[1];
                    r += int64_t(tap.weight) * in[2];
                    a += int64_t(tap.weight) * in[3];
                }
                row[x] = qRgba(toSample(r), toSample(g), toSample(b), toSample(a));
            }
        }
    };

    // Split the passes across up to four threads, since the single-page viewer
    // scales on the GUI thread. Plain std::threads rather than a Qt pool,
    // because in continuous mode this already runs inside ContinuousPageWidget's
    // scale pool.
    const unsigned hw = std::max(1u, std::min(4u, std::thread::hardware_concurrency()));
    if (hw == 1 || (size_t(width) * sh) < 262144) { // smaller than 512x512 after the horizontal pass: not worth the threads
        hPass(0, sh);
        vPass(0, height);
    } else {
        std::vector<std::thread> threads;
        threads.reserve(hw - 1);
        for (unsigned t = 1; t < hw; ++t)
            threads.emplace_back(hPass, int(uint64_t(sh) * t / hw), int(uint64_t(sh) * (t + 1) / hw));
        hPass(0, int(uint64_t(sh) / hw));
        for (auto &t : threads)
            t.join();

        threads.clear();
        for (unsigned t = 1; t < hw; ++t)
            threads.emplace_back(vPass, int(uint64_t(height) * t / hw), int(uint64_t(height) * (t + 1) / hw));
        vPass(0, int(uint64_t(height) / hw));
        for (auto &t : threads)
            t.join();
    }
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

    switch (method) {
    case ScaleMethod::Nearest:
        return image.scaled(width, height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    case ScaleMethod::Bilinear:
        return image.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    case ScaleMethod::Lanczos4:
        return scaleImageLancir(image, width, height, 4.0);
    case ScaleMethod::Bicubic:
        return scaleImageBicubic(image, width, height);
    case ScaleMethod::Lanczos3:
        return scaleImageLancir(image, width, height, 3.0);
    case ScaleMethod::Lanczos2:
        return scaleImageLancir(image, width, height, 2.0);
    }
    return image;
}
