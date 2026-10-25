#include "resize_image.h"

#include <QTest>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

class ResizeImageTest : public QObject
{
    Q_OBJECT

private slots:
    void scalesToTheRequestedSize_data();
    void scalesToTheRequestedSize();
    void returnsANullImageForAnEmptySize_data();
    void returnsANullImageForAnEmptySize();
    void readsOtherFormatsLikeArgb32_data();
    void readsOtherFormatsLikeArgb32();
    void bicubicRingsAndSpreadsLessThanLanczos4WhenEnlarging();
    void bicubicKeepsTheToneOfShrunkLineArt();
    void bicubicKeepsSolidColours();
    void bicubicMatchesADoublePrecisionRun_data();
    void bicubicMatchesADoublePrecisionRun();
};

namespace {

constexpr int dark = 64;
constexpr int light = 192;

const QList<std::pair<const char *, ScaleMethod>> methods {
    { "nearest", ScaleMethod::Nearest },
    { "bilinear", ScaleMethod::Bilinear },
    { "lanczos", ScaleMethod::Lanczos },
    { "bicubic", ScaleMethod::Bicubic },
};

// a vertical edge between two greys far enough from black and white that ringing is not clipped
QImage edge(QSize size)
{
    QImage image(size, QImage::Format_ARGB32);
    image.fill(QColor(dark, dark, dark));
    for (int y = 0; y < size.height(); ++y) {
        for (int x = size.width() / 2; x < size.width(); ++x)
            image.setPixelColor(x, y, QColor(light, light, light));
    }
    return image;
}

QImage enlargedEdge(ScaleMethod method)
{
    return scaleImage(edge(QSize(40, 10)), 160, 40, method);
}

// how many grey levels the scaled edge goes past either side of it
int ringing(const QImage &image)
{
    int worst = 0;
    const int y = image.height() / 2;
    for (int x = 0; x < image.width(); ++x) {
        const int value = qGreen(image.pixel(x, y));
        worst = std::max({ worst, dark - value, value - light });
    }
    return worst;
}

// how many pixels from the edge the scaled edge still differs from the grey on that side
int spread(const QImage &image)
{
    const int y = image.height() / 2;
    const int edgeX = image.width() / 2;
    int widest = 0;
    for (int x = 0; x < image.width(); ++x) {
        const int value = qGreen(image.pixel(x, y));
        if (value != (x < edgeX ? dark : light))
            widest = std::max(widest, x < edgeX ? edgeX - x : x + 1 - edgeX);
    }
    return widest;
}

// Catmull-Rom as bicubic_scale.cpp defines it, in double precision and not rounded
double catmullRom(double t)
{
    t = std::fabs(t);
    if (t <= 1.0)
        return 1.5 * t * t * t - 2.5 * t * t + 1.0;
    if (t < 2.0)
        return -0.5 * t * t * t + 2.5 * t * t - 4.0 * t + 2.0;
    return 0.0;
}

std::vector<std::vector<std::pair<int, double>>> catmullRomWeights(int sourceSize, int targetSize)
{
    std::vector<std::vector<std::pair<int, double>>> weights(targetSize);
    const double scale = double(sourceSize) / targetSize;
    const double widen = std::max(1.0, scale);
    for (int i = 0; i < targetSize; ++i) {
        const double centre = (i + 0.5) * scale - 0.5;
        const int first = std::max(0, int(std::ceil(centre - 2.0 * widen)));
        const int last = std::min(sourceSize - 1, int(std::floor(centre + 2.0 * widen)));
        double sum = 0;
        for (int j = first; j <= last; ++j)
            sum += catmullRom((j - centre) / widen);
        for (int j = first; j <= last; ++j)
            weights[i].push_back({ j, catmullRom((j - centre) / widen) / sum });
    }
    return weights;
}

// every sample of the result, in RGBA order and clamped to 0..255
std::vector<double> doublePrecisionBicubic(const QImage &source, QSize size)
{
    const auto across = catmullRomWeights(source.width(), size.width());
    const auto down = catmullRomWeights(source.height(), size.height());
    std::vector<double> rows(size_t(size.width()) * source.height() * 4);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            double *out = &rows[(size_t(y) * size.width() + x) * 4];
            for (const auto &[from, weight] : across[x]) {
                const QRgb pixel = source.pixel(from, y);
                out[0] += weight * qRed(pixel);
                out[1] += weight * qGreen(pixel);
                out[2] += weight * qBlue(pixel);
                out[3] += weight * qAlpha(pixel);
            }
        }
    }
    std::vector<double> result(size_t(size.width()) * size.height() * 4);
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            double *out = &result[(size_t(y) * size.width() + x) * 4];
            for (const auto &[from, weight] : down[y]) {
                for (int channel = 0; channel < 4; ++channel)
                    out[channel] += weight * rows[(size_t(from) * size.width() + x) * 4 + channel];
            }
            for (int channel = 0; channel < 4; ++channel)
                out[channel] = std::clamp(out[channel], 0.0, 255.0);
        }
    }
    return result;
}

// squares of two greys with noise in every channel, alpha included
QImage noisyTiles(QSize size)
{
    QImage image(size, QImage::Format_ARGB32);
    quint32 seed = 1;
    const auto noise = [&seed](int range) {
        seed = seed * 1664525u + 1013904223u;
        return int((seed >> 16) % quint32(range));
    };
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            const int tile = (x / 25 + y / 25) % 2 ? 200 : 40;
            image.setPixel(x, y, qRgba(std::clamp(tile + noise(41) - 20, 0, 255), noise(256), x % 256, 255 - noise(128)));
        }
    }
    return image;
}

}

void ResizeImageTest::scalesToTheRequestedSize_data()
{
    QTest::addColumn<ScaleMethod>("method");
    QTest::addColumn<QSize>("size");

    for (const auto &[name, method] : methods) {
        QTest::addRow("%s enlarging", name) << method << QSize(70, 50);
        QTest::addRow("%s shrinking", name) << method << QSize(17, 11);
    }
}

void ResizeImageTest::scalesToTheRequestedSize()
{
    QFETCH(ScaleMethod, method);
    QFETCH(QSize, size);

    QCOMPARE(scaleImage(edge(QSize(40, 30)), size.width(), size.height(), method).size(), size);
}

void ResizeImageTest::returnsANullImageForAnEmptySize_data()
{
    QTest::addColumn<ScaleMethod>("method");

    for (const auto &[name, method] : methods)
        QTest::addRow("%s", name) << method;
}

void ResizeImageTest::returnsANullImageForAnEmptySize()
{
    QFETCH(ScaleMethod, method);

    const QImage image = edge(QSize(40, 30));
    QVERIFY(scaleImage(image, 0, 20, method).isNull());
    QVERIFY(scaleImage(image, 20, 0, method).isNull());
    QVERIFY(scaleImage(image, -5, 20, method).isNull());
}

void ResizeImageTest::readsOtherFormatsLikeArgb32_data()
{
    QTest::addColumn<ScaleMethod>("method");
    QTest::addColumn<QImage::Format>("format");

    const QList<std::pair<const char *, QImage::Format>> formats {
        { "RGB32", QImage::Format_RGB32 },
        { "RGB888", QImage::Format_RGB888 },
        { "Grayscale8", QImage::Format_Grayscale8 },
        { "ARGB32 premultiplied", QImage::Format_ARGB32_Premultiplied },
    };
    for (const auto &[name, format] : formats) {
        QTest::addRow("lanczos %s", name) << ScaleMethod::Lanczos << format;
        QTest::addRow("bicubic %s", name) << ScaleMethod::Bicubic << format;
    }
}

void ResizeImageTest::readsOtherFormatsLikeArgb32()
{
    QFETCH(ScaleMethod, method);
    QFETCH(QImage::Format, format);

    const QImage image = noisyTiles(QSize(120, 90)).convertToFormat(format);
    const QImage argb32 = image.convertToFormat(QImage::Format_ARGB32);
    QCOMPARE(scaleImage(image, 170, 61, method), scaleImage(argb32, 170, 61, method));
}

void ResizeImageTest::bicubicRingsAndSpreadsLessThanLanczos4WhenEnlarging()
{
    const QImage bicubic = enlargedEdge(ScaleMethod::Bicubic);
    const QImage lanczos4 = enlargedEdge(ScaleMethod::Lanczos);
    const QImage bilinear = enlargedEdge(ScaleMethod::Bilinear);
    qInfo("grey levels past the edge / pixels from the edge that change: bicubic %d / %d, lanczos4 %d / %d, bilinear %d / %d",
          ringing(bicubic), spread(bicubic), ringing(lanczos4), spread(lanczos4), ringing(bilinear), spread(bilinear));

    QCOMPARE_LT(ringing(bicubic), ringing(lanczos4));
    QCOMPARE_LT(spread(bicubic), spread(lanczos4));
}

// Averaging the stored sRGB values, like the other methods, keeps thin lines as dark as they are with them;
// linear light would turn these lines grey 213 instead of about 170.
void ResizeImageTest::bicubicKeepsTheToneOfShrunkLineArt()
{
    QImage lines(90, 30, QImage::Format_ARGB32);
    lines.fill(Qt::white);
    for (int y = 0; y < lines.height(); ++y) {
        for (int x = 1; x < lines.width(); x += 3)
            lines.setPixelColor(x, y, Qt::black);
    }

    const auto meanGrey = [&lines](ScaleMethod method) {
        const QImage shrunk = scaleImage(lines, 30, 10, method);
        double sum = 0;
        for (int y = 0; y < shrunk.height(); ++y) {
            for (int x = 0; x < shrunk.width(); ++x)
                sum += qGreen(shrunk.pixel(x, y));
        }
        return sum / (shrunk.width() * shrunk.height());
    };

    const double bicubic = meanGrey(ScaleMethod::Bicubic);
    const double bilinear = meanGrey(ScaleMethod::Bilinear);
    qInfo("mean grey of the shrunk lines: bicubic %.1f, bilinear %.1f, lanczos4 %.1f", bicubic, bilinear, meanGrey(ScaleMethod::Lanczos));
    QVERIFY2(std::abs(bicubic - bilinear) < 3, qPrintable(QString("bicubic %1, bilinear %2").arg(bicubic).arg(bilinear)));
}

void ResizeImageTest::bicubicKeepsSolidColours()
{
    for (int level = 0; level < 256; ++level) {
        const QColor colour(level, 255 - level, level / 2, 255 - level / 3);
        QImage image(37, 23, QImage::Format_ARGB32);
        image.fill(colour);
        for (const QSize size : { QSize(101, 67), QSize(13, 9) }) {
            QImage expected(size, QImage::Format_ARGB32);
            expected.fill(colour);
            QVERIFY2(scaleImage(image, size.width(), size.height(), ScaleMethod::Bicubic) == expected,
                     qPrintable(QString("level %1 at %2x%3").arg(level).arg(size.width()).arg(size.height())));
        }
    }
}

void ResizeImageTest::bicubicMatchesADoublePrecisionRun_data()
{
    QTest::addColumn<QSize>("source");
    QTest::addColumn<QSize>("target");

    // the first two are big enough to be split across threads; shrinking 12x reads
    // 48 source rows for each output row, so the ring of rows wraps many times
    QTest::newRow("enlarging") << QSize(640, 480) << QSize(800, 600);
    QTest::newRow("shrinking") << QSize(1040, 720) << QSize(416, 288);
    QTest::newRow("small") << QSize(200, 150) << QSize(83, 61);
    QTest::newRow("shrinking 12x") << QSize(1200, 900) << QSize(100, 75);
}

void ResizeImageTest::bicubicMatchesADoublePrecisionRun()
{
    QFETCH(QSize, source);
    QFETCH(QSize, target);

    const QImage image = noisyTiles(source);
    const QImage scaled = scaleImage(image, target.width(), target.height(), ScaleMethod::Bicubic);
    const std::vector<double> reference = doublePrecisionBicubic(image, target);

    double worst = 0;
    for (int y = 0; y < target.height(); ++y) {
        for (int x = 0; x < target.width(); ++x) {
            const QRgb pixel = scaled.pixel(x, y);
            const double *exact = &reference[(size_t(y) * target.width() + x) * 4];
            const int samples[4] = { qRed(pixel), qGreen(pixel), qBlue(pixel), qAlpha(pixel) };
            for (int channel = 0; channel < 4; ++channel)
                worst = std::max(worst, std::abs(samples[channel] - exact[channel]));
        }
    }
    qInfo("worst difference from the exact value: %.3f", worst);
    // Rounding once is 0.5 off at most, plus a little for the 16.16 weights (up
    // to 0.515 here). Rounding between the passes as well came to 0.7-1.1.
    QCOMPARE_LT(worst, 0.6);
}

QTEST_GUILESS_MAIN(ResizeImageTest)

#include "main.moc"
