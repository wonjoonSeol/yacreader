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
    void bicubicRingsAndSpreadsLessThanLanczos4WhenEnlarging();
    void bicubicKeepsTheToneOfShrunkLineArt();
    void bicubicKeepsSolidColours();
    void bicubicMatchesADoublePrecisionRun_data();
    void bicubicMatchesADoublePrecisionRun();
};

namespace {

constexpr int dark = 64;
constexpr int light = 192;

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

// Catmull-Rom as resize_image.cpp defines it, in double precision and rounded once at the end
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

QImage doublePrecisionBicubic(const QImage &source, QSize size)
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
    QImage result(size, QImage::Format_ARGB32);
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            double sum[4] = { };
            for (const auto &[from, weight] : down[y]) {
                for (int channel = 0; channel < 4; ++channel)
                    sum[channel] += weight * rows[(size_t(from) * size.width() + x) * 4 + channel];
            }
            const auto level = [&](int channel) { return std::clamp(int(std::lround(sum[channel])), 0, 255); };
            result.setPixel(x, y, qRgba(level(0), level(1), level(2), level(3)));
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

    const QList<std::pair<const char *, ScaleMethod>> methods {
        { "nearest", ScaleMethod::Nearest },
        { "bilinear", ScaleMethod::Bilinear },
        { "lanczos", ScaleMethod::Lanczos },
        { "bicubic", ScaleMethod::Bicubic },
    };
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

void ResizeImageTest::bicubicRingsAndSpreadsLessThanLanczos4WhenEnlarging()
{
    const QImage bicubic = enlargedEdge(ScaleMethod::Bicubic);
    const QImage lanczos4 = enlargedEdge(ScaleMethod::Lanczos);

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

    // the larger ones are big enough for the passes to be split across threads
    QTest::newRow("enlarging") << QSize(640, 480) << QSize(800, 600);
    QTest::newRow("shrinking") << QSize(1040, 720) << QSize(416, 288);
    QTest::newRow("small") << QSize(200, 150) << QSize(83, 61);
}

void ResizeImageTest::bicubicMatchesADoublePrecisionRun()
{
    QFETCH(QSize, source);
    QFETCH(QSize, target);

    const QImage image = noisyTiles(source);
    const QImage scaled = scaleImage(image, target.width(), target.height(), ScaleMethod::Bicubic);
    const QImage reference = doublePrecisionBicubic(image, target);

    int worst = 0;
    qint64 off = 0;
    for (int y = 0; y < target.height(); ++y) {
        for (int x = 0; x < target.width(); ++x) {
            const QRgb a = scaled.pixel(x, y);
            const QRgb b = reference.pixel(x, y);
            for (const int difference : { qRed(a) - qRed(b), qGreen(a) - qGreen(b), qBlue(a) - qBlue(b), qAlpha(a) - qAlpha(b) }) {
                worst = std::max(worst, std::abs(difference));
                off += difference != 0;
            }
        }
    }
    // fixed point leaves about 1 % of the samples one level off; rounding between the passes left 10-16 %
    QCOMPARE_LE(worst, 1);
    QCOMPARE_LT(double(off) / (4.0 * target.width() * target.height()), 0.03);
}

QTEST_GUILESS_MAIN(ResizeImageTest)

#include "main.moc"
