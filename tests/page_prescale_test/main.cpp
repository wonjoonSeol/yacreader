#include "page_fit.h"
#include "page_prescaler.h"
#include "page_view.h"

#include <QColor>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSemaphore>
#include <QSignalSpy>
#include <QStringList>
#include <QTest>
#include <QThread>

#include <atomic>
#include <functional>
#include <memory>

namespace {

QStringList describe(const QList<PageView> &views)
{
    QStringList descriptions;
    for (const PageView &view : views) {
        descriptions << QString::number(view.firstPage) + (view.doublePage ? "+" + QString::number(view.firstPage + 1) : QString());
    }
    return descriptions;
}

QImage filled(QSize size, const QColor &color, QImage::Format format = QImage::Format_RGB32)
{
    QImage image(size, format);
    image.fill(color);
    return image;
}

// A request for a single page whose target width tells the views apart in a stage log
PrescaleRequest pageRequest(int page)
{
    PageViewImages images { page, filled(QSize(4, 4), Qt::gray), QImage(), 0 };
    const PrescaleKey key { images.key(), QSize(page + 10, 8), 1.0, ScaleMethod::Lanczos };
    return { key, images };
}

class StageLog
{
public:
    void append(const QString &entry)
    {
        QMutexLocker locker(&mutex);
        entries << entry;
    }
    QStringList snapshot()
    {
        QMutexLocker locker(&mutex);
        return entries;
    }
    // waits without running the event loop, so no result is delivered meanwhile
    bool waitForEntries(int count)
    {
        for (int i = 0; i < 500 && snapshot().size() < count; ++i) {
            QThread::msleep(10);
        }
        return snapshot().size() >= count;
    }

private:
    QMutex mutex;
    QStringList entries;
};

// Records the views it processes by their target width, and waits for a permit before each one
class RecordingStage : public PrescaleStage
{
public:
    RecordingStage(QString name, StageLog *log, QSemaphore *entered, QSemaphore *gate)
        : name(std::move(name)), log(log), entered(entered), gate(gate)
    {
    }

    QImage process(const QImage &, QSize pixelSize, ScaleMethod) const override
    {
        entered->release();
        gate->acquire();
        log->append(QString("%1:%2").arg(name).arg(pixelSize.width() - 10));
        return filled(pixelSize, Qt::white);
    }

private:
    QString name;
    StageLog *log;
    QSemaphore *entered;
    QSemaphore *gate;
};

class ColorStage : public PrescaleStage
{
public:
    explicit ColorStage(QList<QColor> colors)
        : colors(std::move(colors))
    {
    }

    // each call gives the next color
    QImage process(const QImage &, QSize pixelSize, ScaleMethod) const override
    {
        const int call = calls++;
        finished.release();
        return filled(pixelSize, colors.at(qMin(call, colors.size() - 1)));
    }

    mutable QSemaphore finished;

private:
    QList<QColor> colors;
    mutable std::atomic_int calls = 0;
};

}

class PagePrescaleTest : public QObject
{
    Q_OBJECT

private slots:
    void fitsTheWholePageInTheView();
    void fitsTheWidthOrTheHeight();
    void enlargesSmallPagesOnlyWhenAsked();
    void keepsTheFullResolution();
    void zoomsAfterFitting();
    void scalesThePixelSizeByTheDevicePixelRatio();
    void fitsDoublePagesByTheirCombinedSize();
    void laysOutDoublePagesInReadingOrder_data();
    void laysOutDoublePagesInReadingOrder();
    void drawsDoublePagesInTheirLayout();
    void ordersNearbySinglePagesNextThenPrevious();
    void leavesOutSinglePagesPastTheEnds();
    void predictsDoublePagesFromThePagePairing();
    void replaysTheDoublePageHistory();
    void leavesOutViewsWithPagesNotDecoded();
    void findsImagesOnlyForTheSameKey();
    void keepsOnlyTheScheduledViews();
    void deliversImagesReadyForAPixmap();
    void startsWorkInTheScheduledOrder();
    void runsEveryFirstStageBeforeLaterStages();
    void replacesImagesWithLaterStages();
    void dropsResultsFromBeforeAnInvalidation();
};

void PagePrescaleTest::fitsTheWholePageInTheView()
{
    const PageFitOptions options { QSize(1000, 1000), YACReader::FitMode::FullPage };

    QCOMPARE(fitPage(QSize(1600, 2400), options).size, QSize(666, 1000));
    QCOMPARE(fitPage(QSize(2400, 1600), options).size, QSize(1000, 666));
}

void PagePrescaleTest::fitsTheWidthOrTheHeight()
{
    const QSize view(1000, 700);

    QCOMPARE(fitPage(QSize(1600, 2400), { view, YACReader::FitMode::ToWidth }).size, QSize(1000, 1500));
    QCOMPARE(fitPage(QSize(1600, 2400), { view, YACReader::FitMode::ToHeight }).size, QSize(466, 700));
}

void PagePrescaleTest::enlargesSmallPagesOnlyWhenAsked()
{
    const QSize view(1000, 700);
    const QSize page(400, 600);

    QCOMPARE(fitPage(page, { view, YACReader::FitMode::ToWidth, false }).size, page);
    QCOMPARE(fitPage(page, { view, YACReader::FitMode::ToWidth, true }).size, QSize(1000, 1500));
    QCOMPARE(fitPage(page, { view, YACReader::FitMode::ToHeight, false }).size, page);
    QCOMPARE(fitPage(page, { view, YACReader::FitMode::ToHeight, true }).size, QSize(466, 700));
    // fitting the whole page always enlarges
    QCOMPARE(fitPage(page, { view, YACReader::FitMode::FullPage, false }).size, QSize(466, 700));
}

void PagePrescaleTest::keepsTheFullResolution()
{
    QCOMPARE(fitPage(QSize(1600, 2400), { QSize(1000, 700), YACReader::FitMode::FullRes }).size, QSize(1600, 2400));
}

void PagePrescaleTest::zoomsAfterFitting()
{
    const QSize page(1600, 2400);
    const QSize view(1000, 1000);

    QCOMPARE(fitPage(page, { view, YACReader::FitMode::FullPage, true, 150 }).size, QSize(999, 1500));
    QCOMPARE(fitPage(page, { view, YACReader::FitMode::FullPage, true, 50 }).size, QSize(333, 500));
}

void PagePrescaleTest::scalesThePixelSizeByTheDevicePixelRatio()
{
    const PageFit fit = fitPage(QSize(1600, 2400), { QSize(1000, 1000), YACReader::FitMode::FullPage, true, 100, 1.5 });

    QCOMPARE(fit.size, QSize(666, 1000));
    QCOMPARE(fit.pixelSize, QSize(999, 1500));
}

void PagePrescaleTest::fitsDoublePagesByTheirCombinedSize()
{
    const PageFitOptions options { QSize(1000, 1000), YACReader::FitMode::FullPage };
    // rotated pages are decoded sideways and stacked
    const QSize sideBySide = doublePageLayout(QSize(1600, 2400), QSize(1600, 2400), 0).size;
    const QSize stacked = doublePageLayout(QSize(2400, 1600), QSize(2400, 1600), 90).size;

    QCOMPARE(sideBySide, QSize(3200, 2400));
    QCOMPARE(fitPage(sideBySide, options).size, QSize(1000, 750));
    QCOMPARE(stacked, QSize(2400, 3200));
    QCOMPARE(fitPage(stacked, options).size, QSize(750, 1000));
}

void PagePrescaleTest::laysOutDoublePagesInReadingOrder_data()
{
    QTest::addColumn<int>("rotation");
    QTest::addColumn<QSize>("firstSize");
    QTest::addColumn<QSize>("secondSize");
    QTest::addColumn<QSize>("size");
    QTest::addColumn<QRect>("firstRect");
    QTest::addColumn<QRect>("secondRect");
    QTest::addColumn<qreal>("seamRatio");
    QTest::addColumn<bool>("horizontalSeam");

    // the shorter second page grows to the height of the first one
    QTest::newRow("upright") << 0 << QSize(1000, 1500) << QSize(800, 1000) << QSize(2200, 1500)
                             << QRect(0, 0, 1000, 1500) << QRect(1000, 0, 1200, 1500) << 1000 / 2200.0 << false;
    QTest::newRow("upside down") << 180 << QSize(1000, 1500) << QSize(800, 1000) << QSize(2200, 1500)
                                 << QRect(1200, 0, 1000, 1500) << QRect(0, 0, 1200, 1500) << 1200 / 2200.0 << false;
    QTest::newRow("turned right") << 90 << QSize(1500, 1000) << QSize(1000, 800) << QSize(1500, 2200)
                                  << QRect(0, 0, 1500, 1000) << QRect(0, 1000, 1500, 1200) << 1000 / 2200.0 << true;
    QTest::newRow("turned left") << 270 << QSize(1500, 1000) << QSize(1000, 800) << QSize(1500, 2200)
                                 << QRect(0, 1200, 1500, 1000) << QRect(0, 0, 1500, 1200) << 1200 / 2200.0 << true;
}

void PagePrescaleTest::laysOutDoublePagesInReadingOrder()
{
    QFETCH(int, rotation);
    QFETCH(QSize, firstSize);
    QFETCH(QSize, secondSize);

    const DoublePageLayout layout = doublePageLayout(firstSize, secondSize, rotation);

    QTEST(layout.size, "size");
    QTEST(layout.firstRect, "firstRect");
    QTEST(layout.secondRect, "secondRect");
    QTEST(layout.seamRatio, "seamRatio");
    QTEST(layout.seamOrientation == Qt::Horizontal, "horizontalSeam");
}

void PagePrescaleTest::drawsDoublePagesInTheirLayout()
{
    const QImage first = filled(QSize(10, 20), Qt::red);
    const QImage second = filled(QSize(5, 10), Qt::blue);

    const QImage upright = PageViewImages { 0, first, second, 0 }.image();
    const QImage upsideDown = PageViewImages { 0, first, second, 180 }.image();

    QCOMPARE(upright.size(), QSize(20, 20));
    QCOMPARE(upright.pixelColor(5, 10), QColor(Qt::red));
    QCOMPARE(upright.pixelColor(15, 19), QColor(Qt::blue));
    QCOMPARE(upsideDown.pixelColor(5, 10), QColor(Qt::blue));
    QCOMPARE(upsideDown.pixelColor(15, 19), QColor(Qt::red));
    QCOMPARE(PageViewImages({ 0, first, QImage(), 0 }).image(), first);
}

void PagePrescaleTest::ordersNearbySinglePagesNextThenPrevious()
{
    const auto unused = [](int) { return PagePairing::Unknown; };

    QCOMPARE(describe(nearbyPageViews({ 5, false }, 10, false, { }, unused)), QStringList({ "6", "7", "4" }));
}

void PagePrescaleTest::leavesOutSinglePagesPastTheEnds()
{
    const auto unused = [](int) { return PagePairing::Unknown; };

    QCOMPARE(describe(nearbyPageViews({ 0, false }, 10, false, { }, unused)), QStringList({ "1", "2" }));
    QCOMPARE(describe(nearbyPageViews({ 8, false }, 10, false, { }, unused)), QStringList({ "9", "7" }));
    QCOMPARE(describe(nearbyPageViews({ 9, false }, 10, false, { }, unused)), QStringList({ "8" }));
}

void PagePrescaleTest::predictsDoublePagesFromThePagePairing()
{
    // the cover is shown alone, and so is the landscape page 5
    const auto pairingAt = [](int page) {
        return page == 0 || page == 4 || page == 5 ? PagePairing::Single : PagePairing::Double;
    };

    QCOMPARE(describe(nearbyPageViews({ 1, true }, 10, true, { }, pairingAt)), QStringList({ "3+4", "5", "0" }));
    QCOMPARE(describe(nearbyPageViews({ 6, true }, 10, true, { }, pairingAt)), QStringList({ "8+9", "5" }));
}

void PagePrescaleTest::replaysTheDoublePageHistory()
{
    const auto pairingAt = [](int) { return PagePairing::Double; };
    // page 4 was not decoded when page 3 was shown, so the view after 3 started at 4
    const PageViewHistory history { { 3, 4, 6 }, 1 };

    QCOMPARE(describe(nearbyPageViews({ 4, true }, 10, true, history, pairingAt)), QStringList({ "6+7", "8+9", "3" }));
    // a history that does not track the current view is not replayed
    QCOMPARE(describe(nearbyPageViews({ 5, true }, 10, true, history, pairingAt)), QStringList({ "7+8", "9", "3+4" }));
}

void PagePrescaleTest::leavesOutViewsWithPagesNotDecoded()
{
    const auto pageSixMissing = [](int page) { return page == 5 || page == 6 ? PagePairing::Unknown : PagePairing::Double; };
    const auto pageTwoMissing = [](int page) { return page == 1 || page == 2 ? PagePairing::Unknown : PagePairing::Double; };

    QCOMPARE(describe(nearbyPageViews({ 3, true }, 10, true, { }, pageSixMissing)), QStringList({ "1+2" }));
    QCOMPARE(describe(nearbyPageViews({ 3, true }, 10, true, { }, pageTwoMissing)), QStringList({ "5+6", "7+8" }));
}

void PagePrescaleTest::findsImagesOnlyForTheSameKey()
{
    PagePrescaler prescaler({ std::make_shared<ScaleStage>() }, 1);
    const PrescaleKey key { { 3, 11, 12 }, QSize(40, 60), 2.0, ScaleMethod::Lanczos };
    const QImage image = filled(QSize(40, 60), Qt::red);
    prescaler.store(key, image);

    const auto findsWith = [&](const std::function<void(PrescaleKey &)> &change) {
        PrescaleKey other = key;
        change(other);
        return !prescaler.image(other).isNull();
    };

    QCOMPARE(prescaler.image(key), image);
    QVERIFY(!findsWith([](PrescaleKey &other) { other.view.firstPage = 4; }));
    QVERIFY(!findsWith([](PrescaleKey &other) { other.view.firstImageKey = 21; }));
    QVERIFY(!findsWith([](PrescaleKey &other) { other.view.secondImageKey = 22; }));
    QVERIFY(!findsWith([](PrescaleKey &other) { other.pixelSize = QSize(41, 60); }));
    QVERIFY(!findsWith([](PrescaleKey &other) { other.devicePixelRatio = 1.0; }));
    QVERIFY(!findsWith([](PrescaleKey &other) { other.scaleMethod = ScaleMethod::Bilinear; }));
}

void PagePrescaleTest::keepsOnlyTheScheduledViews()
{
    PagePrescaler prescaler({ std::make_shared<ScaleStage>() }, 1);
    const PrescaleRequest first = pageRequest(1);
    const PrescaleRequest second = pageRequest(2);
    const PrescaleRequest third = pageRequest(3);
    prescaler.store(first.key, filled(first.key.pixelSize, Qt::red));
    prescaler.store(second.key, filled(second.key.pixelSize, Qt::red));

    prescaler.schedule({ second, third });

    QVERIFY(prescaler.image(first.key).isNull());
    QCOMPARE(prescaler.image(second.key).pixelColor(0, 0), QColor(Qt::red));
    QTRY_VERIFY(!prescaler.image(third.key).isNull());
}

void PagePrescaleTest::deliversImagesReadyForAPixmap()
{
    PagePrescaler prescaler({ std::make_shared<ScaleStage>() }, 1);
    PrescaleRequest opaque = pageRequest(1);
    opaque.images.first = filled(QSize(4, 4), Qt::gray, QImage::Format_ARGB32);
    opaque.key = { opaque.images.key(), QSize(8, 8), 2.0, ScaleMethod::Lanczos };
    PrescaleRequest transparent = pageRequest(2);
    transparent.images.first = filled(QSize(4, 4), Qt::transparent, QImage::Format_ARGB32);
    transparent.key = { transparent.images.key(), QSize(8, 8), 2.0, ScaleMethod::Lanczos };

    prescaler.schedule({ opaque, transparent });

    QTRY_VERIFY(!prescaler.image(opaque.key).isNull() && !prescaler.image(transparent.key).isNull());
    QCOMPARE(prescaler.image(opaque.key).format(), QImage::Format_RGB32);
    QCOMPARE(prescaler.image(opaque.key).devicePixelRatio(), 2.0);
    QCOMPARE(prescaler.image(transparent.key).format(), QImage::Format_ARGB32_Premultiplied);
}

void PagePrescaleTest::startsWorkInTheScheduledOrder()
{
    StageLog log;
    QSemaphore entered;
    QSemaphore gate;
    PagePrescaler prescaler({ std::make_shared<RecordingStage>("scale", &log, &entered, &gate) }, 1);
    const PrescaleRequest page3 = pageRequest(3);
    const PrescaleRequest page4 = pageRequest(4);
    const PrescaleRequest page5 = pageRequest(5);
    const PrescaleRequest page6 = pageRequest(6);
    const PrescaleRequest page7 = pageRequest(7);

    // current, next, the one after, previous
    prescaler.schedule({ page5, page6, page7, page4 });
    entered.acquire();
    // going back to page 4 while page 5 is being scaled
    prescaler.schedule({ page4, page5, page6, page3 });
    gate.release(10);

    QVERIFY(log.waitForEntries(4));
    QCOMPARE(log.snapshot(), QStringList({ "scale:5", "scale:4", "scale:6", "scale:3" }));
}

void PagePrescaleTest::runsEveryFirstStageBeforeLaterStages()
{
    StageLog log;
    QSemaphore entered;
    QSemaphore gate;
    PagePrescaler prescaler({ std::make_shared<RecordingStage>("fast", &log, &entered, &gate),
                              std::make_shared<RecordingStage>("slow", &log, &entered, &gate) },
                            1);
    const PrescaleRequest page9 = pageRequest(9);
    const PrescaleRequest page1 = pageRequest(1);
    const PrescaleRequest page2 = pageRequest(2);
    const PrescaleRequest page3 = pageRequest(3);

    prescaler.schedule({ page9 });
    entered.acquire();
    // page 1 only misses the slow stage
    prescaler.store(page1.key, filled(page1.key.pixelSize, Qt::red));
    prescaler.schedule({ page9, page1, page2, page3 });
    gate.release(10);

    QVERIFY(log.waitForEntries(4));
    QCOMPARE(log.snapshot().mid(0, 4), QStringList({ "fast:9", "fast:2", "fast:3", "slow:1" }));
}

void PagePrescaleTest::replacesImagesWithLaterStages()
{
    PagePrescaler prescaler({ std::make_shared<ColorStage>(QList<QColor> { Qt::red }), std::make_shared<ColorStage>(QList<QColor> { Qt::blue }) }, 1);
    QSignalSpy improved(&prescaler, &PagePrescaler::imageImproved);
    const PrescaleRequest page = pageRequest(1);

    prescaler.schedule({ page });

    QTRY_COMPARE(improved.count(), 2);
    QCOMPARE(improved.at(1).at(0).value<PrescaleKey>(), page.key);
    QCOMPARE(prescaler.image(page.key).pixelColor(0, 0), QColor(Qt::blue));
    prescaler.store(page.key, filled(page.key.pixelSize, Qt::green));
    QCOMPARE(prescaler.image(page.key).pixelColor(0, 0), QColor(Qt::blue));
}

void PagePrescaleTest::dropsResultsFromBeforeAnInvalidation()
{
    auto stage = std::make_shared<ColorStage>(QList<QColor> { Qt::red, Qt::blue });
    PagePrescaler prescaler({ stage }, 1);
    QSignalSpy improved(&prescaler, &PagePrescaler::imageImproved);
    const PrescaleRequest page = pageRequest(1);

    prescaler.schedule({ page });
    // the red result is on its way back once the stage is done
    stage->finished.acquire();
    QThread::msleep(100);
    prescaler.invalidate();
    prescaler.schedule({ page });

    QTRY_COMPARE(improved.count(), 1);
    QTest::qWait(100);
    QCOMPARE(improved.count(), 1);
    QCOMPARE(prescaler.image(page.key).pixelColor(0, 0), QColor(Qt::blue));
}

QTEST_GUILESS_MAIN(PagePrescaleTest)

#include "main.moc"
