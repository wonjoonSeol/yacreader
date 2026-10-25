#include "bicubic_scale.h"

#include <QThread>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <system_error>
#include <thread>
#include <vector>

// Separable Catmull-Rom resampler. It averages the stored sRGB values, like the
// other methods: linear light would make shrunk line art and screentones
// lighter than with Bilinear or Lanczos, and it deepens the dark halo beside
// enlarged edges.

namespace {

// How much of one source pixel goes into an output pixel, in 16.16 fixed point.
struct Tap {
    int index;
    int weight;
};

// The taps of every output pixel along one axis, in source order. The weights
// of each output pixel sum to exactly 1 << 16.
using Taps = std::vector<std::vector<Tap>>;

double catmullRom(double t)
{
    t = std::fabs(t);
    if (t <= 1.0)
        return 1.5 * t * t * t - 2.5 * t * t + 1.0;
    if (t < 2.0)
        return -0.5 * t * t * t + 2.5 * t * t - 4.0 * t + 2.0;
    return 0.0;
}

// When shrinking, the kernel is widened by the scale factor, so each output
// pixel averages the whole source area it covers and moire stays down. Taps past
// the image border are dropped and the others renormalised.
Taps tapsFor(int sourceSize, int targetSize)
{
    Taps taps(targetSize);
    const double scale = double(sourceSize) / targetSize;
    const double widen = std::max(1.0, scale);
    std::vector<double> kernel;
    for (int i = 0; i < targetSize; ++i) {
        const double centre = (i + 0.5) * scale - 0.5;
        const int first = std::max(0, int(std::ceil(centre - 2.0 * widen)));
        const int last = std::min(sourceSize - 1, int(std::floor(centre + 2.0 * widen)));
        kernel.clear();
        double sum = 0.0;
        for (int j = first; j <= last; ++j) {
            kernel.push_back(catmullRom((j - centre) / widen));
            sum += kernel.back();
        }

        auto &pixel = taps[i];
        int total = 0;
        for (int j = first; j <= last; ++j) {
            const double weight = kernel[j - first];
            if (weight == 0.0)
                continue;
            pixel.push_back({ j, int(std::lround(weight / sum * 65536.0)) });
            total += pixel.back().weight;
        }
        // rounding the weights one by one leaves their sum a few units off
        const auto largest = std::max_element(pixel.begin(), pixel.end(), [](const Tap &a, const Tap &b) { return a.weight < b.weight; });
        largest->weight += 65536 - total;
    }
    return taps;
}

// the most source rows that one output row reads
int mostRowsRead(const Taps &taps)
{
    int most = 1;
    for (const auto &pixel : taps)
        most = std::max(most, pixel.back().index - pixel.front().index + 1);
    return most;
}

// Rounds a horizontal sum (16.16 weights x 8-bit samples) to the signed 8.6
// sample kept between the passes. Catmull-Rom's overshoot has to reach the
// vertical pass unclipped, or the negative lobes cannot cancel out there. The
// sums stay within about -32 to 287 grey levels, so the clamp only guards the
// cast.
int16_t toIntermediate(int sum)
{
    return int16_t(std::clamp((sum + (1 << 9)) >> 10, -(511 << 6), 511 << 6));
}

// Rounds a vertical sum (16.16 weights x 8.6 samples, so 22 fraction bits) to
// an 8-bit sample.
uint8_t toSample(int64_t sum)
{
    return uint8_t(std::clamp(int((sum + (1 << 21)) >> 22), 0, 255));
}

struct Resampler {
    const QImage &source;
    const Taps across;
    const Taps down;
    // QImage::scanLine() detaches the image, which is not safe from several
    // threads at once, so the rows are written through these
    uchar *const targetBits;
    const qsizetype targetBytesPerLine;
    const int width;
    const int ringRows;

    // Scales the output rows [begin, end). The horizontal pass fills a ring of
    // rows just ahead of the vertical pass, so only the rows that the next
    // output row reads are kept, never the whole image scaled across.
    void scaleRows(int begin, int end) const
    {
        if (begin >= end)
            return;
        const size_t rowSize = size_t(width) * 4;
        const auto ring = std::make_unique_for_overwrite<int16_t[]>(size_t(ringRows) * rowSize);
        const auto ringRow = [&](int sourceRow) { return &ring[size_t(sourceRow % ringRows) * rowSize]; };

        std::vector<const int16_t *> rows;
        int nextSourceRow = down[begin].front().index;
        for (int y = begin; y < end; ++y) {
            for (; nextSourceRow <= down[y].back().index; ++nextSourceRow)
                scaleAcross(nextSourceRow, ringRow(nextSourceRow));
            rows.clear();
            for (const Tap &tap : down[y])
                rows.push_back(ringRow(tap.index));
            scaleDown(y, rows);
        }
    }

    void scaleAcross(int sourceRow, int16_t *out) const
    {
        const auto *in = reinterpret_cast<const QRgb *>(source.constScanLine(sourceRow));
        for (int x = 0; x < width; ++x, out += 4) {
            int b = 0, g = 0, r = 0, a = 0;
            for (const Tap &tap : across[x]) {
                const QRgb pixel = in[tap.index];
                b += tap.weight * qBlue(pixel);
                g += tap.weight * qGreen(pixel);
                r += tap.weight * qRed(pixel);
                a += tap.weight * qAlpha(pixel);
            }
            out[0] = toIntermediate(b);
            out[1] = toIntermediate(g);
            out[2] = toIntermediate(r);
            out[3] = toIntermediate(a);
        }
    }

    // rows holds the scaled-across source rows that down[targetRow] reads, in tap order
    void scaleDown(int targetRow, const std::vector<const int16_t *> &rows) const
    {
        const auto &taps = down[targetRow];
        auto *out = reinterpret_cast<QRgb *>(targetBits + targetRow * targetBytesPerLine);
        for (int x = 0; x < width; ++x) {
            int64_t b = 0, g = 0, r = 0, a = 0;
            for (size_t i = 0; i < taps.size(); ++i) {
                const int16_t *sample = rows[i] + size_t(x) * 4;
                const int64_t weight = taps[i].weight;
                b += weight * sample[0];
                g += weight * sample[1];
                r += weight * sample[2];
                a += weight * sample[3];
            }
            out[x] = qRgba(toSample(r), toSample(g), toSample(b), toSample(a));
        }
    }
};

// Only the GUI thread splits the work, for the single-page viewer. Continuous
// scroll already scales several pages at once in ContinuousPageWidget's pool,
// which keeps to half the cores on purpose.
int threadCount(qsizetype pixelsScaledAcross)
{
    constexpr qsizetype minPixelsToSplit = 512 * 512;
    if (!QThread::isMainThread() || pixelsScaledAcross < minPixelsToSplit)
        return 1;
    return int(std::clamp(std::thread::hardware_concurrency(), 1u, 4u));
}

}

QImage scaleImageBicubic(const QImage &image, int width, int height)
{
    QImage target(width, height, QImage::Format_ARGB32);
    if (target.isNull())
        return target;

    Taps down = tapsFor(image.height(), height);
    const int ringRows = mostRowsRead(down);
    const Resampler resampler { image, tapsFor(image.width(), width), std::move(down), target.bits(), target.bytesPerLine(), width, ringRows };

    const int bands = std::min(height, threadCount(qsizetype(width) * image.height()));
    {
        std::vector<std::jthread> helpers; // joined when this block ends
        for (int band = 1; band < bands; ++band) {
            const int begin = height * band / bands;
            const int end = height * (band + 1) / bands;
            try {
                helpers.emplace_back([&resampler, begin, end] { resampler.scaleRows(begin, end); });
            } catch (const std::system_error &) {
                resampler.scaleRows(begin, end); // no thread to spare: scale this band here
            }
        }
        resampler.scaleRows(0, height / bands);
    }
    return target;
}
