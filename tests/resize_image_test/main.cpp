#include "resize_image.h"

#include <QColor>
#include <QImage>
#include <QTest>

#include <algorithm>
#include <cstdlib>

class ResizeImageTest : public QObject
{
    Q_OBJECT

private slots:
    void scalesEveryPixelFormat_data();
    void scalesEveryPixelFormat();
    void keepsTheAlpha_data();
    void keepsTheAlpha();
    void scalesFromAndToOnePixel_data();
    void scalesFromAndToOnePixel();
    void givesANullImageForAnEmptySize_data();
    void givesANullImageForAnEmptySize();
    void fewerLobesSpreadLessFromAnEdge();
    void lanczos2RingsLessThanLanczos4WhenEnlarging();
    void readsTheStoredValues();
    void readsNoMethodFromOtherValues();
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
    { "Lanczos4", ScaleMethod::Lanczos, 2 },
    { "Lanczos3", ScaleMethod::Lanczos3, 4 },
    { "Lanczos2", ScaleMethod::Lanczos2, 5 },
};

constexpr int dark = 64;
constexpr int light = 192;

const QColor orange(200, 100, 50);

QImage filled(QSize size, QImage::Format format, const QColor &color)
{
    QImage image(size, format);
    image.fill(color);
    return image;
}

// the largest difference from color in any channel of any pixel, alpha included
int largestDifference(const QImage &image, const QColor &color)
{
    int largest = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            largest = std::max({ largest,
                                 std::abs(pixel.red() - color.red()),
                                 std::abs(pixel.green() - color.green()),
                                 std::abs(pixel.blue() - color.blue()),
                                 std::abs(pixel.alpha() - color.alpha()) });
        }
    }
    return largest;
}

// a vertical edge between two greys far enough from black and white that ringing is not clipped
QImage edge(QSize size)
{
    QImage image = filled(size, QImage::Format_ARGB32, QColor(dark, dark, dark));
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

}

void ResizeImageTest::scalesEveryPixelFormat_data()
{
    QTest::addColumn<ScaleMethod>("method");
    QTest::addColumn<QImage::Format>("format");
    QTest::addColumn<QColor>("color");

    for (const auto &entry : methods) {
        QTest::addRow("%s RGB32", entry.name) << entry.method << QImage::Format_RGB32 << orange;
        QTest::addRow("%s ARGB32", entry.name) << entry.method << QImage::Format_ARGB32 << orange;
        QTest::addRow("%s ARGB32 premultiplied", entry.name) << entry.method << QImage::Format_ARGB32_Premultiplied << orange;
        QTest::addRow("%s Grayscale8", entry.name) << entry.method << QImage::Format_Grayscale8 << QColor(128, 128, 128);
    }
}

// a flat page stays flat, so every output pixel was written and the channels were read in the right order
void ResizeImageTest::scalesEveryPixelFormat()
{
    QFETCH(ScaleMethod, method);
    QFETCH(QImage::Format, format);
    QFETCH(QColor, color);

    const QImage scaled = scaleImage(filled(QSize(30, 20), format, color), 70, 50, method);

    QCOMPARE(scaled.size(), QSize(70, 50));
    QCOMPARE_LE(largestDifference(scaled, color), 1);
}

void ResizeImageTest::keepsTheAlpha_data()
{
    QTest::addColumn<ScaleMethod>("method");
    QTest::addColumn<QColor>("color");

    for (const auto &entry : methods) {
        QTest::addRow("%s transparent", entry.name) << entry.method << QColor(Qt::transparent);
        QTest::addRow("%s half transparent", entry.name) << entry.method << QColor(200, 100, 50, 128);
    }
}

void ResizeImageTest::keepsTheAlpha()
{
    QFETCH(ScaleMethod, method);
    QFETCH(QColor, color);

    const QImage page = filled(QSize(30, 20), QImage::Format_ARGB32, color);

    for (const QSize size : { QSize(70, 50), QSize(12, 8) }) {
        const QImage scaled = scaleImage(page, size.width(), size.height(), method);

        QCOMPARE(scaled.size(), size);
        // Qt's smooth scaling goes through premultiplied alpha, which rounds the colour by up to 1
        QCOMPARE_LE(largestDifference(scaled, color), 1);
    }
}

void ResizeImageTest::scalesFromAndToOnePixel_data()
{
    QTest::addColumn<ScaleMethod>("method");
    QTest::addColumn<QSize>("from");
    QTest::addColumn<QSize>("to");

    for (const auto &entry : methods) {
        QTest::addRow("%s from one pixel", entry.name) << entry.method << QSize(1, 1) << QSize(4, 3);
        QTest::addRow("%s to one pixel", entry.name) << entry.method << QSize(4, 3) << QSize(1, 1);
    }
}

void ResizeImageTest::scalesFromAndToOnePixel()
{
    QFETCH(ScaleMethod, method);
    QFETCH(QSize, from);
    QFETCH(QSize, to);

    const QImage scaled = scaleImage(filled(from, QImage::Format_ARGB32, orange), to.width(), to.height(), method);

    QCOMPARE(scaled.size(), to);
    QCOMPARE_LE(largestDifference(scaled, orange), 1);
}

void ResizeImageTest::givesANullImageForAnEmptySize_data()
{
    QTest::addColumn<ScaleMethod>("method");
    QTest::addColumn<QSize>("size");

    for (const auto &entry : methods) {
        QTest::addRow("%s no width", entry.name) << entry.method << QSize(0, 30);
        QTest::addRow("%s no height", entry.name) << entry.method << QSize(40, 0);
    }
}

void ResizeImageTest::givesANullImageForAnEmptySize()
{
    QFETCH(ScaleMethod, method);
    QFETCH(QSize, size);

    QVERIFY(scaleImage(edge(QSize(20, 10)), size.width(), size.height(), method).isNull());
}

// Swapping any two of the lobe counts breaks this order
void ResizeImageTest::fewerLobesSpreadLessFromAnEdge()
{
    const int lanczos4 = spread(enlargedEdge(ScaleMethod::Lanczos));
    const int lanczos3 = spread(enlargedEdge(ScaleMethod::Lanczos3));
    const int lanczos2 = spread(enlargedEdge(ScaleMethod::Lanczos2));

    QCOMPARE_LT(lanczos2, lanczos3);
    QCOMPARE_LT(lanczos3, lanczos4);
}

void ResizeImageTest::lanczos2RingsLessThanLanczos4WhenEnlarging()
{
    const int lanczos2 = ringing(enlargedEdge(ScaleMethod::Lanczos2));
    const int lanczos4 = ringing(enlargedEdge(ScaleMethod::Lanczos));

    QCOMPARE_LT(lanczos2, lanczos4);
}

void ResizeImageTest::readsTheStoredValues()
{
    for (const auto &entry : methods) {
        QCOMPARE(static_cast<int>(entry.method), entry.storedValue);
        QCOMPARE(scaleMethodFromInt(entry.storedValue), std::optional(entry.method));
    }
}

// 3 is left free for Bicubic, and 6 is what a build with one more method could store
void ResizeImageTest::readsNoMethodFromOtherValues()
{
    for (const int value : { -1, 3, 6 })
        QVERIFY(!scaleMethodFromInt(value).has_value());
}

QTEST_GUILESS_MAIN(ResizeImageTest)

#include "main.moc"
