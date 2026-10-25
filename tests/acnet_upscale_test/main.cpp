#include "AC/Core.hpp"
#include "AC/Util/Parallel.hpp"
#include "acnet_upscale.h"
#include "resize_image.h"

#include <QElapsedTimer>
#include <QTest>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

using AcnetVariant = ac::core::model::ACNet<8>::Variant;

Q_DECLARE_METATYPE(AcnetModel)
Q_DECLARE_METATYPE(AcnetVariant)

class AcnetUpscaleTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void doublesTheSize_data();
    void doublesTheSize();
    void matchesAnime4kcppOnTheWholeImage_data();
    void matchesAnime4kcppOnTheWholeImage();
    void resultDoesNotDependOnTheThreadCount();
    void keepsTransparencyAndColourAtAlphaEdges();
    void beatsLanczosOnLineArt();
    void parallelForStaysWithinTheThreadLimit();
    void reportsTheTime();
};

namespace {

// Line art at its full size: white paper, black strokes of several widths and a patch of
// diagonal hatching, antialiased by distance to the stroke.
QImage lineArt(int width, int height)
{
    std::vector<float> ink(size_t(width) * height, 0.0f);
    auto stroke = [&](int x, int y, float distance, float halfWidth) {
        float &pixel = ink[size_t(y) * width + x];
        pixel = std::max(pixel, std::clamp(halfWidth + 0.5f - distance, 0.0f, 1.0f));
    };

    struct Ring {
        float cx, cy, radius, halfWidth;
    };
    const Ring rings[] = {
        { 0.30f, 0.25f, 0.20f, 0.8f }, { 0.30f, 0.25f, 0.12f, 2.5f }, { 0.72f, 0.30f, 0.15f, 1.2f }, { 0.50f, 0.70f, 0.30f, 4.0f }, { 0.50f, 0.70f, 0.22f, 0.7f }, { 0.25f, 0.85f, 0.08f, 1.6f }
    };
    for (const Ring &ring : rings) {
        const float cx = ring.cx * width, cy = ring.cy * height, r = ring.radius * width;
        const int margin = int(r + ring.halfWidth + 2);
        for (int y = std::max(0, int(cy) - margin); y < std::min(height, int(cy) + margin); ++y)
            for (int x = std::max(0, int(cx) - margin); x < std::min(width, int(cx) + margin); ++x)
                stroke(x, y, std::abs(std::hypot(x + 0.5f - cx, y + 0.5f - cy) - r), ring.halfWidth);
    }

    const float spacing = width / 60.0f;
    for (int y = height / 10; y < height / 2; ++y)
        for (int x = width / 2; x < width * 9 / 10; ++x) {
            const float t = (x + 0.5f + y + 0.5f) / std::sqrt(2.0f);
            stroke(x, y, std::abs(t - spacing * std::round(t / spacing)), 0.75f);
        }

    for (int x = 0; x < width; ++x) {
        const float curve = height * (0.5f + 0.06f * std::sin(x * 6.0f / width));
        for (int y = std::max(0, int(curve) - 6); y < std::min(height, int(curve) + 6); ++y)
            stroke(x, y, std::abs(y + 0.5f - curve), 1.5f);
    }

    QImage image(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        uchar *line = image.scanLine(y);
        for (int x = 0; x < width; ++x)
            line[x] = uchar(std::lround(255.0f * (1.0f - ink[size_t(y) * width + x])));
    }
    return image;
}

// A colour page: the line art inked over a smooth gradient.
QImage colourPage(int width, int height)
{
    const QImage art = lineArt(width, height);
    QImage image(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        const uchar *ink = art.constScanLine(y);
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            const float paper = ink[x] / 255.0f;
            line[x] = qRgb(int(paper * (120 + 135 * x / width)), int(paper * (200 - 80 * y / height)), int(paper * 230));
        }
    }
    return image;
}

QImage boxShrink2x(const QImage &image)
{
    QImage small(image.width() / 2, image.height() / 2, QImage::Format_Grayscale8);
    for (int y = 0; y < small.height(); ++y) {
        const uchar *top = image.constScanLine(2 * y), *bottom = image.constScanLine(2 * y + 1);
        uchar *line = small.scanLine(y);
        for (int x = 0; x < small.width(); ++x)
            line[x] = uchar((top[2 * x] + top[2 * x + 1] + bottom[2 * x] + bottom[2 * x + 1] + 2) / 4);
    }
    return small;
}

double psnr(const QImage &a, const QImage &b)
{
    const QImage x = a.convertToFormat(QImage::Format_Grayscale8), y = b.convertToFormat(QImage::Format_Grayscale8);
    double squaredError = 0;
    for (int row = 0; row < x.height(); ++row)
        for (int column = 0; column < x.width(); ++column) {
            const double difference = x.constScanLine(row)[column] - y.constScanLine(row)[column];
            squaredError += difference * difference;
        }
    return 10.0 * std::log10(255.0 * 255.0 * x.width() * x.height() / squaredError);
}

// Anime4KCPP's own whole-image path, as the reference for the wrapper's strips and formats.
QImage anime4kcppUpscale(const QImage &image, AcnetVariant variant)
{
    const QImage::Format format = image.format() == QImage::Format_Grayscale8 ? QImage::Format_Grayscale8
            : image.hasAlphaChannel()                                         ? QImage::Format_RGBA8888
                                                                              : QImage::Format_RGB888;
    const QImage src = image.convertToFormat(format);
    const ac::core::Image in(src.width(), src.height(), src.depth() / 8, ac::core::Image::UInt8, const_cast<uchar *>(src.constBits()), int(src.bytesPerLine()));

    const auto processor = ac::core::Processor::create<ac::core::Processor::CPU>(0, ac::core::model::ACNet<8> { variant });
    const ac::core::Image out = processor->process(in, 2.0);

    QImage result(out.width(), out.height(), format);
    for (int y = 0; y < out.height(); ++y)
        std::memcpy(result.scanLine(y), out.line(y), size_t(out.width()) * out.channels());
    return format == QImage::Format_Grayscale8 ? result
            : image.hasAlphaChannel()          ? result.convertToFormat(QImage::Format_ARGB32)
                                               : result.convertToFormat(QImage::Format_RGB32);
}

QImage withTransparentHalf(const QImage &image)
{
    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < result.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x)
            if (x > result.width() / 2)
                line[x] = qRgba(qRed(line[x]), qGreen(line[x]), qBlue(line[x]), 255 * (y % 64) / 63);
    }
    return result;
}

}

void AcnetUpscaleTest::initTestCase()
{
    const auto processor = ac::core::Processor::create<ac::core::Processor::CPU>(0, ac::core::model::ACNet<8> { AcnetVariant::B4_NORMAL });
    qInfo("Anime4KCPP CPU backend: %s", processor->name());
}

void AcnetUpscaleTest::cleanup()
{
    setAcnetMaxThreads(0);
}

void AcnetUpscaleTest::doublesTheSize_data()
{
    QTest::addColumn<QImage>("image");
    QTest::addColumn<QImage::Format>("expectedFormat");

    const QImage gray = lineArt(37, 23);
    QTest::newRow("grayscale") << gray << QImage::Format_Grayscale8;
    QTest::newRow("indexed gray") << gray.convertToFormat(QImage::Format_Indexed8) << QImage::Format_Grayscale8;
    QTest::newRow("rgb32") << colourPage(37, 23) << QImage::Format_RGB32;
    QTest::newRow("rgb888") << colourPage(37, 23).convertToFormat(QImage::Format_RGB888) << QImage::Format_RGB32;
    QTest::newRow("argb32") << withTransparentHalf(colourPage(37, 23)) << QImage::Format_ARGB32;
    QTest::newRow("premultiplied") << withTransparentHalf(colourPage(37, 23)).convertToFormat(QImage::Format_ARGB32_Premultiplied) << QImage::Format_ARGB32;
    QTest::newRow("one pixel") << colourPage(1, 1) << QImage::Format_RGB32;
}

void AcnetUpscaleTest::doublesTheSize()
{
    QFETCH(QImage, image);
    QFETCH(QImage::Format, expectedFormat);

    const QImage result = upscale2xAcnet(image);

    QCOMPARE(result.size(), image.size() * 2);
    QCOMPARE(result.format(), expectedFormat);
    QVERIFY(upscale2xAcnet(QImage()).isNull());
}

void AcnetUpscaleTest::matchesAnime4kcppOnTheWholeImage_data()
{
    QTest::addColumn<QImage>("image");
    QTest::addColumn<AcnetModel>("model");
    QTest::addColumn<AcnetVariant>("variant");

    // Taller than several strips and not a multiple of the strip height.
    const QImage gray = lineArt(300, 1100);
    const QImage colour = colourPage(300, 1100);
    QTest::newRow("gray f8b4") << gray << AcnetModel::F8B4 << AcnetVariant::B4_NORMAL;
    QTest::newRow("gray f8b8-hdn") << gray << AcnetModel::F8B8Hdn << AcnetVariant::B8_HDN;
    QTest::newRow("colour f8b4") << colour << AcnetModel::F8B4 << AcnetVariant::B4_NORMAL;
    QTest::newRow("alpha f8b8-hdn") << withTransparentHalf(colour) << AcnetModel::F8B8Hdn << AcnetVariant::B8_HDN;
}

void AcnetUpscaleTest::matchesAnime4kcppOnTheWholeImage()
{
    QFETCH(QImage, image);
    QFETCH(AcnetModel, model);
    QFETCH(AcnetVariant, variant);

    QCOMPARE(upscale2xAcnet(image, model), anime4kcppUpscale(image, variant));
}

void AcnetUpscaleTest::resultDoesNotDependOnTheThreadCount()
{
    const QImage page = colourPage(200, 300);

    setAcnetMaxThreads(1);
    const QImage singleThreaded = upscale2xAcnet(page);
    setAcnetMaxThreads(3);

    QCOMPARE(upscale2xAcnet(page), singleThreaded);
}

void AcnetUpscaleTest::keepsTransparencyAndColourAtAlphaEdges()
{
    // A red disc on a fully transparent background, transparent pixels carrying green.
    QImage image(64, 48, QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const float coverage = std::clamp(16.5f - std::hypot(x + 0.5f - 32, y + 0.5f - 24), 0.0f, 1.0f);
            image.setPixel(x, y, coverage > 0 ? qRgba(220, 20, 20, int(std::lround(255 * coverage))) : qRgba(0, 255, 0, 0));
        }

    const QImage result = upscale2xAcnet(image);

    QCOMPARE(qAlpha(result.pixel(2, 2)), 0);
    QCOMPARE(qAlpha(result.pixel(64, 48)), 255);
    for (int y = 0; y < result.height(); ++y)
        for (int x = 0; x < result.width(); ++x) {
            const QRgb pixel = result.pixel(x, y);
            if (qAlpha(pixel) < 64)
                continue;
            // Premultiplied processing: no green bleeding in from the transparent background.
            QVERIFY2(qRed(pixel) > 150 && qGreen(pixel) < 80 && qBlue(pixel) < 80,
                     qPrintable(QStringLiteral("(%1, %2) is #%3").arg(x).arg(y).arg(pixel, 8, 16, QLatin1Char('0'))));
        }
}

void AcnetUpscaleTest::beatsLanczosOnLineArt()
{
    const QImage original = lineArt(800, 1200);
    const QImage small = boxShrink2x(original);

    const double lanczos = psnr(scaleImage(small, original.width(), original.height(), ScaleMethod::Lanczos), original);
    const double fast = psnr(upscale2xAcnet(small, AcnetModel::F8B4), original);
    const double quality = psnr(upscale2xAcnet(small, AcnetModel::F8B8Hdn), original);
    qInfo("Line art 2x PSNR: Lanczos4 %.2f dB, ACNet F8B4 %.2f dB, F8B8-HDN %.2f dB", lanczos, fast, quality);

    QVERIFY2(fast > lanczos + 3.0, "ACNet F8B4 should beat Lanczos4 by more than 3 dB");
    QVERIFY2(quality > lanczos + 3.0, "ACNet F8B8-HDN should beat Lanczos4 by more than 3 dB");
}

void AcnetUpscaleTest::parallelForStaysWithinTheThreadLimit()
{
    constexpr int count = 64;
    std::vector<std::atomic<int>> visits(count);
    std::mutex mutex;
    std::set<std::thread::id> threads;

    ac::util::parallelForThreads = 2;
    ac::util::parallelFor(0, count, [&](int i) {
        visits[i]++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const std::lock_guard lock(mutex);
        threads.insert(std::this_thread::get_id());
    });

    for (int i = 0; i < count; ++i)
        QCOMPARE(visits[i].load(), 1);
    QVERIFY(threads.size() <= 2);
}

void AcnetUpscaleTest::reportsTheTime()
{
    const QImage gray = lineArt(1000, 1500);
    const QImage colour = colourPage(1000, 1500);

    for (const AcnetModel model : { AcnetModel::F8B4, AcnetModel::F8B8Hdn }) {
        for (const QImage *page : { &gray, &colour }) {
            QElapsedTimer timer;
            timer.start();
            const QImage result = upscale2xAcnet(*page, model);
            qInfo("ACNet %s 2x of a 1000x1500 %s page: %lld ms on %d threads",
                  model == AcnetModel::F8B4 ? "F8B4" : "F8B8-HDN", page == &gray ? "gray" : "colour", timer.elapsed(),
                  std::max(1, QThread::idealThreadCount() / 2));
            QCOMPARE(result.width(), 2000);
        }
    }
}

QTEST_GUILESS_MAIN(AcnetUpscaleTest)
#include "main.moc"
