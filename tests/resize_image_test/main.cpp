#include "resize_image.h"

#include <QTest>

#include <algorithm>

class ResizeImageTest : public QObject
{
    Q_OBJECT

private slots:
    void scalesToTheRequestedSize_data();
    void scalesToTheRequestedSize();
    void eachLanczosMethodScalesDifferently();
    void lanczos2RingsLessThanLanczos4WhenEnlarging();
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

}

void ResizeImageTest::scalesToTheRequestedSize_data()
{
    QTest::addColumn<ScaleMethod>("method");

    QTest::newRow("nearest") << ScaleMethod::Nearest;
    QTest::newRow("bilinear") << ScaleMethod::Bilinear;
    QTest::newRow("lanczos4") << ScaleMethod::Lanczos;
    QTest::newRow("lanczos3") << ScaleMethod::Lanczos3;
    QTest::newRow("lanczos2") << ScaleMethod::Lanczos2;
}

void ResizeImageTest::scalesToTheRequestedSize()
{
    QFETCH(ScaleMethod, method);

    QCOMPARE(scaleImage(edge(QSize(40, 30)), 70, 50, method).size(), QSize(70, 50));
}

void ResizeImageTest::eachLanczosMethodScalesDifferently()
{
    const QImage lanczos4 = enlargedEdge(ScaleMethod::Lanczos);
    const QImage lanczos3 = enlargedEdge(ScaleMethod::Lanczos3);
    const QImage lanczos2 = enlargedEdge(ScaleMethod::Lanczos2);

    QVERIFY(lanczos4 != lanczos3);
    QVERIFY(lanczos4 != lanczos2);
    QVERIFY(lanczos3 != lanczos2);
}

void ResizeImageTest::lanczos2RingsLessThanLanczos4WhenEnlarging()
{
    const int lanczos2 = ringing(enlargedEdge(ScaleMethod::Lanczos2));
    const int lanczos4 = ringing(enlargedEdge(ScaleMethod::Lanczos));

    QVERIFY2(lanczos2 < lanczos4, qPrintable(QString("lanczos2 %1, lanczos4 %2").arg(lanczos2).arg(lanczos4)));
}

QTEST_GUILESS_MAIN(ResizeImageTest)

#include "main.moc"
