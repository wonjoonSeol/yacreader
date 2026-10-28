#include "acnet_stage.h"
#include "acnet_upscale.h"
#include "page_prescaler.h"

#include <QSignalSpy>
#include <QTest>

#include <atomic>
#include <memory>
#include <optional>

class AcnetStageTest : public QObject
{
    Q_OBJECT

private slots:
    void tellsWhenItsImagesAreStale_data();
    void tellsWhenItsImagesAreStale();
    void declinesViewsItDoesNotUpscale_data();
    void declinesViewsItDoesNotUpscale();
    void upscalesOneViewAtATime();
    void stopsWhenTheJobIsCancelled();
    void replacesThePlainScaleInThePrescaler();
};

namespace {

// -1 for off
std::optional<AcnetModel> modelOf(int value)
{
    return value < 0 ? std::nullopt : std::optional(static_cast<AcnetModel>(value));
}

QImage colourPage(int width, int height)
{
    QImage image(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            image.setPixel(x, y, (x / 3 + y / 5) % 2 ? qRgb(240, 230, 200) : qRgb(40, 30 + 4 * y, 20 + 3 * x));
    return image;
}

}

void AcnetStageTest::tellsWhenItsImagesAreStale_data()
{
    QTest::addColumn<int>("before");
    QTest::addColumn<int>("after");
    QTest::addColumn<bool>("stale");

    const int off = -1, f8b4 = int(AcnetModel::F8B4), f8b8Hdn = int(AcnetModel::F8B8Hdn);
    QTest::newRow("off, off") << off << off << false;
    // the next schedule asks every view again
    QTest::newRow("turned on") << off << f8b4 << false;
    QTest::newRow("same model") << f8b4 << f8b4 << false;
    QTest::newRow("other model") << f8b4 << f8b8Hdn << true;
    QTest::newRow("turned off") << f8b8Hdn << off << true;
}

void AcnetStageTest::tellsWhenItsImagesAreStale()
{
    QFETCH(int, before);
    QFETCH(int, after);
    QFETCH(bool, stale);
    AcnetStage stage;
    stage.setModel(modelOf(before));

    QCOMPARE(stage.setModel(modelOf(after)), stale);
}

void AcnetStageTest::declinesViewsItDoesNotUpscale_data()
{
    QTest::addColumn<bool>("on");
    QTest::addColumn<QSize>("imageSize");
    QTest::addColumn<QSize>("pixelSize");
    QTest::addColumn<bool>("active");

    QTest::newRow("off") << false << QSize(1000, 1500) << QSize(2000, 3000) << false;
    QTest::newRow("2x") << true << QSize(1000, 1500) << QSize(2000, 3000) << true;
    QTest::newRow("1.4x") << true << QSize(1000, 1500) << QSize(1400, 2100) << false;
    QTest::newRow("double page over 4 MP") << true << QSize(3000, 1500) << QSize(6000, 3000) << false;
}

void AcnetStageTest::declinesViewsItDoesNotUpscale()
{
    QFETCH(bool, on);
    QFETCH(QSize, imageSize);
    QFETCH(QSize, pixelSize);
    QFETCH(bool, active);
    AcnetStage stage;
    stage.setModel(on ? std::optional(AcnetModel::F8B4) : std::nullopt);

    QCOMPARE(stage.isActive(imageSize, pixelSize), active);
}

void AcnetStageTest::upscalesOneViewAtATime()
{
    QCOMPARE(AcnetStage().maxConcurrent(), 1);
}

void AcnetStageTest::stopsWhenTheJobIsCancelled()
{
    AcnetStage stage;
    stage.setModel(AcnetModel::F8B4);
    const std::atomic_bool cancelled = true;

    QVERIFY(stage.process(colourPage(60, 40), QSize(120, 80), ScaleMethod::Lanczos, cancelled).isNull());
}

void AcnetStageTest::replacesThePlainScaleInThePrescaler()
{
    auto stage = std::make_shared<AcnetStage>();
    stage->setModel(AcnetModel::F8B8Hdn);
    PagePrescaler prescaler({ std::make_shared<ScaleStage>(), stage }, 2);
    QSignalSpy improved(&prescaler, &PagePrescaler::imageImproved);
    const QImage page = colourPage(60, 40);
    const PageViewImages images { 0, page, QImage(), 0 };
    const PrescaleKey key { images.key(), QSize(120, 80), 1.0, ScaleMethod::Nearest };

    prescaler.schedule({ { key, images } });

    // the plain scale, then ACNet's
    QTRY_COMPARE_WITH_TIMEOUT(improved.count(), 2, 10000);
    const std::atomic_bool notCancelled = false;
    const QImage expected = scaleImageWithAcnet(page, key.pixelSize, key.scaleMethod, AcnetModel::F8B8Hdn, notCancelled);
    QCOMPARE(prescaler.image(key).convertToFormat(QImage::Format_RGB32), expected.convertToFormat(QImage::Format_RGB32));
}

QTEST_GUILESS_MAIN(AcnetStageTest)
#include "main.moc"
