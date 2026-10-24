#include "resize_image.h"

#include <QColor>
#include <QImage>
#include <QTest>

#include <algorithm>

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

QTEST_GUILESS_MAIN(ResizeImageTest)

#include "main.moc"
