#include "resize_image.h"

#include <QColor>
#include <QImage>
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
    void fewerLobesSpreadLessFromAnEdge();
    void lanczos2RingsLessThanLanczos4WhenEnlarging();
    void readsTheStoredValues();
    void fallsBackToLanczos4ForUnknownValues();
    void bicubicScalesDifferentlyFromTheOtherMethods();
    void bicubicRingsAndSpreadsLessThanLanczos4WhenEnlarging();
    void bicubicKeepsTheToneOfShrunkLineArt();
    void bicubicKeepsSolidColours();
    void bicubicMatchesADoublePrecisionRun_data();
    void bicubicMatchesADoublePrecisionRun();
};

namespace {

struct Method {
    const char *name;
    ScaleMethod method;
    int storedValue;
};

constexpr Method methods[] = {
    { "Nearest", ScaleMethod::Nearest, 0 },
    { "Bilinear", ScaleMethod::Bilinear, 1 },
    { "Lanczos4", ScaleMethod::Lanczos4, 2 },
    { "Bicubic", ScaleMethod::Bicubic, 3 },
    { "Lanczos3", ScaleMethod::Lanczos3, 4 },
    { "Lanczos2", ScaleMethod::Lanczos2, 5 },
};

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

// how many pixels from the edge the scaled image still differs from the flat grey on that side
int spread(const QImage &image)
{
    int furthest = 0;
    const int y = image.height() / 2;
    const int middle = image.width() / 2;
    for (int x = 0; x < image.width(); ++x) {
        const bool darkSide = x < middle;
        if (qGreen(image.pixel(x, y)) != (darkSide ? dark : light))
            furthest = std::max(furthest, darkSide ? middle - x : x - middle + 1);
    }
    return furthest;
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

    for (const auto &entry : methods) {
        QTest::addRow("%s enlarging", entry.name) << entry.method << QSize(70, 50);
        QTest::addRow("%s shrinking", entry.name) << entry.method << QSize(25, 15);
    }
}

void ResizeImageTest::scalesToTheRequestedSize()
{
    QFETCH(ScaleMethod, method);
    QFETCH(QSize, size);

    QCOMPARE(scaleImage(edge(QSize(40, 30)), size.width(), size.height(), method).size(), size);
}

void ResizeImageTest::fewerLobesSpreadLessFromAnEdge()
{
    const int lanczos4 = spread(enlargedEdge(ScaleMethod::Lanczos4));
    const int lanczos3 = spread(enlargedEdge(ScaleMethod::Lanczos3));
    const int lanczos2 = spread(enlargedEdge(ScaleMethod::Lanczos2));

    QVERIFY2(lanczos2 < lanczos3 && lanczos3 < lanczos4,
             qPrintable(QString("lanczos2 %1, lanczos3 %2, lanczos4 %3").arg(lanczos2).arg(lanczos3).arg(lanczos4)));
}

void ResizeImageTest::lanczos2RingsLessThanLanczos4WhenEnlarging()
{
    const int lanczos2 = ringing(enlargedEdge(ScaleMethod::Lanczos2));
    const int lanczos4 = ringing(enlargedEdge(ScaleMethod::Lanczos4));

    QVERIFY2(lanczos2 < lanczos4, qPrintable(QString("lanczos2 %1, lanczos4 %2").arg(lanczos2).arg(lanczos4)));
}

void ResizeImageTest::readsTheStoredValues()
{
    for (const auto &entry : methods) {
        QCOMPARE(static_cast<int>(entry.method), entry.storedValue);
        QCOMPARE(scaleMethodFromSetting(entry.storedValue), entry.method);
        // ini files give the values back as strings
        QCOMPARE(scaleMethodFromSetting(QString::number(entry.storedValue)), entry.method);
    }
}

void ResizeImageTest::fallsBackToLanczos4ForUnknownValues()
{
    for (const QVariant &value : { QVariant(), QVariant(-1), QVariant(6), QVariant("Lanczos3") })
        QCOMPARE(scaleMethodFromSetting(value), ScaleMethod::Lanczos4);
}

void ResizeImageTest::bicubicScalesDifferentlyFromTheOtherMethods()
{
    const QImage bicubic = enlargedEdge(ScaleMethod::Bicubic);

    QVERIFY(bicubic != enlargedEdge(ScaleMethod::Bilinear));
    QVERIFY(bicubic != enlargedEdge(ScaleMethod::Lanczos4));
}

void ResizeImageTest::bicubicRingsAndSpreadsLessThanLanczos4WhenEnlarging()
{
    const QImage bicubic = enlargedEdge(ScaleMethod::Bicubic);
    const QImage lanczos4 = enlargedEdge(ScaleMethod::Lanczos4);

    QVERIFY2(ringing(bicubic) < ringing(lanczos4), qPrintable(QString("bicubic %1, lanczos4 %2").arg(ringing(bicubic)).arg(ringing(lanczos4))));
    QVERIFY2(spread(bicubic) < spread(lanczos4), qPrintable(QString("bicubic %1 px, lanczos4 %2 px").arg(spread(bicubic)).arg(spread(lanczos4))));
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
            const QImage scaled = scaleImage(image, size.width(), size.height(), ScaleMethod::Bicubic);
            for (int y = 0; y < scaled.height(); ++y) {
                for (int x = 0; x < scaled.width(); ++x) {
                    if (scaled.pixel(x, y) != colour.rgba())
                        QFAIL(qPrintable(QString("level %1 at %2x%3 became %4").arg(level).arg(size.width()).arg(size.height()).arg(scaled.pixel(x, y), 8, 16)));
                }
            }
        }
    }
}

void ResizeImageTest::bicubicMatchesADoublePrecisionRun_data()
{
    QTest::addColumn<QSize>("source");
    QTest::addColumn<QSize>("target");

    // the larger ones are big enough for the passes to be split across threads
    QTest::newRow("enlarging") << QSize(640, 480) << QSize(800, 600);
    QTest::newRow("shrinking") << QSize(1280, 960) << QSize(512, 384);
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
    const double offShare = double(off) / (4.0 * target.width() * target.height());
    QVERIFY2(worst <= 1, qPrintable(QString("worst difference %1").arg(worst)));
    QVERIFY2(offShare < 0.03, qPrintable(QString("%1 % of the samples differ").arg(100 * offShare)));
}

QTEST_GUILESS_MAIN(ResizeImageTest)

#include "main.moc"
