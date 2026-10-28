#include "page_fit.h"
#include "page_prescaler.h"
#include "page_view.h"

#include <QColor>
#include <QCoreApplication>
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

constexpr int kTimeoutMs = 5000;

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

PrescaleRequest sizedRequest(int page, QSize pixelSize)
{
    PageViewImages images { page, filled(QSize(4, 4), Qt::gray), QImage(), 0 };
    const PrescaleKey key { images.key(), pixelSize, 1.0, ScaleMethod::Lanczos };
    return { key, images };
}

// A request for a single page whose target width tells the views apart in a stage log
PrescaleRequest pageRequest(int page)
{
    return sizedRequest(page, QSize(page + 10, 8));
}

PrescaleRequest doublePageRequest(int page)
{
    PageViewImages images { page, filled(QSize(10, 20), Qt::red), filled(QSize(10, 20), Qt::blue), 0 };
    const PrescaleKey key { images.key(), QSize(10, 10), 1.0, ScaleMethod::Lanczos };
    return { key, images };
}

// scaleNow() runs the first stage on the test thread
bool onPoolThread()
{
    return QThread::currentThread() != QCoreApplication::instance()->thread();
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
        for (int i = 0; i < kTimeoutMs / 10 && snapshot().size() < count; ++i) {
            QThread::msleep(10);
        }
        return snapshot().size() >= count;
    }

private:
    QMutex mutex;
    QStringList entries;
};

// On pool threads, records the views it processes by their target width and waits for a permit before each one
class RecordingStage : public PrescaleStage
{
public:
    RecordingStage(QString name, StageLog *log, QSemaphore *entered, QSemaphore *gate)
        : name(std::move(name)), log(log), entered(entered), gate(gate)
    {
    }

    QImage process(const QImage &, QSize pixelSize, ScaleMethod, const std::atomic_bool &) const override
    {
        if (onPoolThread()) {
            entered->release();
            gate->tryAcquire(1, kTimeoutMs);
            log->append(QString("%1:%2").arg(name).arg(pixelSize.width() - 10));
        }
        return filled(pixelSize, Qt::white);
    }

private:
    QString name;
    StageLog *log;
    QSemaphore *entered;
    QSemaphore *gate;
};

// Each call gives the next color, and an invalid color gives a null image
class ColorStage : public PrescaleStage
{
public:
    explicit ColorStage(QList<QColor> colors)
        : colors(std::move(colors))
    {
    }

    QImage process(const QImage &image, QSize pixelSize, ScaleMethod, const std::atomic_bool &) const override
    {
        entered.release();
        {
            QMutexLocker locker(&mutex);
            inputKeys << image.cacheKey();
        }
        const QColor color = colors.at(qMin(calls++, colors.size() - 1));
        return color.isValid() ? filled(pixelSize, color) : QImage();
    }

    QList<qint64> inputs() const
    {
        QMutexLocker locker(&mutex);
        return inputKeys;
    }

    mutable QSemaphore entered;
    mutable std::atomic_int calls = 0;

private:
    QList<QColor> colors;
    mutable QMutex mutex;
    mutable QList<qint64> inputKeys;
};

// On pool threads, works until it is cancelled and then gives red; scaleNow() gets blue at once
class CancellableStage : public PrescaleStage
{
public:
    QImage process(const QImage &, QSize pixelSize, ScaleMethod, const std::atomic_bool &cancelled) const override
    {
        if (!onPoolThread()) {
            return filled(pixelSize, Qt::blue);
        }
        entered.release();
        for (int i = 0; i < kTimeoutMs / 10 && !cancelled; ++i) {
            QThread::msleep(10);
        }
        if (cancelled) {
            sawCancel.release();
        }
        return filled(pixelSize, Qt::red);
    }

    mutable QSemaphore entered;
    mutable QSemaphore sawCancel;
};

// Works on one view at a time and waits for a permit for each, recording how many it ever had at once
class OneAtATimeStage : public PrescaleStage
{
public:
    int maxConcurrent() const override { return 1; }

    QImage process(const QImage &, QSize pixelSize, ScaleMethod, const std::atomic_bool &) const override
    {
        const int running = ++current;
        mostAtOnce = qMax(mostAtOnce.load(), running);
        entered.release();
        gate.tryAcquire(1, kTimeoutMs);
        --current;
        return filled(pixelSize, Qt::green);
    }

    mutable QSemaphore entered;
    mutable QSemaphore gate;
    mutable std::atomic_int mostAtOnce = 0;

private:
    mutable std::atomic_int current = 0;
};

class DecliningStage : public ColorStage
{
public:
    DecliningStage()
        : ColorStage({ Qt::yellow })
    {
    }

    bool isActive(QSize imageSize, QSize) const override
    {
        askedSize = imageSize;
        return false;
    }

    // isActive() is called on the GUI thread
    mutable QSize askedSize;
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
    void laysOutMangaPagesTheOtherWayRound();
    void ordersNearbySinglePagesNextThenPrevious();
    void leavesOutSinglePagesPastTheEnds();
    void predictsDoublePagesFromThePagePairing();
    void replaysTheDoublePageHistory();
    void leavesOutViewsWithPagesNotDecoded();
    void predictsTheViewsThePageTurnsShow();
    void findsImagesOnlyForTheSameKey();
    void keepsOnlyTheScheduledViews();
    void keepsTheViewsThatFitTheByteBudget();
    void keepsOneViewScaledNowOutsideTheSchedule();
    void deliversImagesReadyForAPixmap();
    void startsWorkInTheScheduledOrder();
    void runsEveryFirstStageBeforeLaterStages();
    void replacesImagesWithLaterStages();
    void keepsThePreviousImageWhenAStageGivesNone();
    void scalesNowAfterTheBackgroundGaveNoImage();
    void cancelsTheBackgroundWorkScaleNowDoes();
    void cancelsRunningWorkWhenDestroyed();
    void dropsResultsFromBeforeAnInvalidation();
    void skipsStagesThatDecline();
    void drawsADoublePageOnceForAllStages();
    void limitsTheViewsAStageWorksOnAtOnce();
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
    const FittedPage fit = fitPage(QSize(1600, 2400), { QSize(1000, 1000), YACReader::FitMode::FullPage, true, 100, 1.5 });

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
    QVERIFY(PageViewImages({ 0, first, second, 0 }).key() != PageViewImages({ 0, first, second, 180 }).key());
    QCOMPARE(PageViewImages({ 0, first, QImage(), 0 }).image(), first);
}

void PagePrescaleTest::laysOutMangaPagesTheOtherWayRound()
{
    const QImage page = filled(QSize(10, 20), Qt::red);
    const QImage nextPage = filled(QSize(5, 10), Qt::blue);

    const PageViewImages western = PageViewImages::of(4, page, nextPage, false, 0);
    const PageViewImages manga = PageViewImages::of(4, page, nextPage, true, 0);

    QCOMPARE(western.image().pixelColor(5, 10), QColor(Qt::red));
    QCOMPARE(western.image().pixelColor(15, 10), QColor(Qt::blue));
    QCOMPARE(manga.image().pixelColor(5, 10), QColor(Qt::blue));
    QCOMPARE(manga.image().pixelColor(15, 10), QColor(Qt::red));
    QVERIFY(manga.key() != western.key());
    // a single page has no order
    QCOMPARE(PageViewImages::of(4, page, QImage(), true, 0).key(), PageViewImages::of(4, page, QImage(), false, 0).key());
}

void PagePrescaleTest::ordersNearbySinglePagesNextThenPrevious()
{
    const auto unused = [](int) { return PagePairing::Unknown; };

    QCOMPARE(describe(nearbyPageViews(5, 10, false, { }, unused)), QStringList({ "6", "7", "4" }));
}

void PagePrescaleTest::leavesOutSinglePagesPastTheEnds()
{
    const auto unused = [](int) { return PagePairing::Unknown; };

    QCOMPARE(describe(nearbyPageViews(0, 10, false, { }, unused)), QStringList({ "1", "2" }));
    QCOMPARE(describe(nearbyPageViews(8, 10, false, { }, unused)), QStringList({ "9", "7" }));
    QCOMPARE(describe(nearbyPageViews(9, 10, false, { }, unused)), QStringList({ "8" }));
}

void PagePrescaleTest::predictsDoublePagesFromThePagePairing()
{
    // the cover is shown alone, and so is the landscape page 5
    const auto pairingAt = [](int page) {
        return page == 0 || page == 4 || page == 5 ? PagePairing::Single : PagePairing::Double;
    };

    QCOMPARE(describe(nearbyPageViews(1, 10, true, { }, pairingAt)), QStringList({ "3+4", "5", "0" }));
    QCOMPARE(describe(nearbyPageViews(6, 10, true, { }, pairingAt)), QStringList({ "8+9", "5" }));
}

void PagePrescaleTest::replaysTheDoublePageHistory()
{
    const auto pairingAt = [](int) { return PagePairing::Double; };
    // page 4 was not decoded yet when the reader turned from page 3, so the view after 3 started at 4
    DoublePageNavigation navigation;
    navigation.reset(3);
    QCOMPARE(navigation.next(3, 10, [](int) { return PagePairing::Unknown; }), 4);
    QCOMPARE(navigation.next(4, 10, pairingAt), 6);
    QCOMPARE(navigation.previous(6, pairingAt), 4);

    QCOMPARE(describe(nearbyPageViews(4, 10, true, navigation, pairingAt)), QStringList({ "6+7", "8+9", "3" }));
    // a history that does not track the current view is not replayed
    QCOMPARE(describe(nearbyPageViews(5, 10, true, navigation, pairingAt)), QStringList({ "7+8", "9", "3+4" }));
}

void PagePrescaleTest::leavesOutViewsWithPagesNotDecoded()
{
    const auto pageSixMissing = [](int page) { return page == 5 || page == 6 ? PagePairing::Unknown : PagePairing::Double; };
    const auto pageTwoMissing = [](int page) { return page == 1 || page == 2 ? PagePairing::Unknown : PagePairing::Double; };

    QCOMPARE(describe(nearbyPageViews(3, 10, true, { }, pageSixMissing)), QStringList({ "1+2" }));
    QCOMPARE(describe(nearbyPageViews(3, 10, true, { }, pageTwoMissing)), QStringList({ "5+6", "7+8" }));
}

// Turns the pages the way Render does in double page mode, with its navigation and current page, and checks every
// view a page turn shows against what nearbyPageViews() predicted for it before the turn
void PagePrescaleTest::predictsTheViewsThePageTurnsShow()
{
    const int numPages = 10;
    // page 4 is decoded only after the third page turn, and page 6 is a landscape page
    bool pageFourDecoded = false;
    const PagePairingFunction pairingAt = [&pageFourDecoded](int page) {
        if (!pageFourDecoded && (page == 3 || page == 4)) {
            return PagePairing::Unknown;
        }
        return page == 0 || page == 5 || page == 6 ? PagePairing::Single : PagePairing::Double;
    };
    const QList<int> turns { 1, 1, 1, 1, -1, -1, -1, 1, 1, 1, 1, 1 };

    DoublePageNavigation navigation;
    int page = 0;
    QStringList shown;
    QStringList aheadBefore;
    int compared = 0;
    for (int turn = 0; turn < turns.size(); ++turn) {
        QList<PageView> ahead;
        QList<PageView> behind;
        for (const PageView &view : nearbyPageViews(page, numPages, true, navigation, pairingAt)) {
            (view.firstPage > page ? ahead : behind).append(view);
        }

        const bool forward = turns.at(turn) > 0;
        page = forward ? navigation.next(page, numPages, pairingAt) : navigation.previous(page, pairingAt);
        pageFourDecoded = pageFourDecoded || turn == 2;
        const QString view = describe({ PageView { page, navigation.showsTwoPages(page, numPages, pairingAt) } }).first();
        shown << view;

        const QStringList predicted = describe(forward ? ahead : behind);
        if (!predicted.isEmpty()) {
            QCOMPARE(view, predicted.first());
            ++compared;
        }
        // two turns ahead, as predicted before the turn before this one
        if (forward && turn > 0 && turns.at(turn - 1) > 0 && aheadBefore.size() == 2) {
            QCOMPARE(view, aheadBefore.at(1));
            ++compared;
        }
        aheadBefore = describe(ahead);
    }

    // going back to page 3 keeps it alone, as it was shown while page 4 was not decoded
    QCOMPARE(shown, QStringList({ "1+2", "3", "4+5", "6", "4+5", "3", "1+2", "3", "4+5", "6", "7+8", "9" }));
    QCOMPARE(compared, 14);
}

void PagePrescaleTest::findsImagesOnlyForTheSameKey()
{
    PagePrescaler prescaler({ std::make_shared<ScaleStage>() }, 1);
    const PrescaleKey key { { 3, 11, 12, 90 }, QSize(40, 60), 2.0, ScaleMethod::Lanczos };
    const QImage image = prescaler.scaleNow({ key, { 3, filled(QSize(4, 6), Qt::red), QImage(), 90 } });

    const auto findsWith = [&](const std::function<void(PrescaleKey &)> &change) {
        PrescaleKey other = key;
        change(other);
        return !prescaler.image(other).isNull();
    };

    QVERIFY(!image.isNull());
    QCOMPARE(prescaler.image(key), image);
    QVERIFY(!findsWith([](PrescaleKey &other) { other.view.firstPage = 4; }));
    QVERIFY(!findsWith([](PrescaleKey &other) { other.view.firstImageKey = 21; }));
    QVERIFY(!findsWith([](PrescaleKey &other) { other.view.secondImageKey = 22; }));
    QVERIFY(!findsWith([](PrescaleKey &other) { other.view.rotation = 0; }));
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
    prescaler.schedule({ first, second });
    QTRY_VERIFY(!prescaler.image(first.key).isNull() && !prescaler.image(second.key).isNull());
    const qint64 secondImage = prescaler.image(second.key).cacheKey();

    prescaler.schedule({ second, third });

    QVERIFY(prescaler.image(first.key).isNull());
    QCOMPARE(prescaler.image(second.key).cacheKey(), secondImage);
    QTRY_VERIFY(!prescaler.image(third.key).isNull());
}

void PagePrescaleTest::keepsTheViewsThatFitTheByteBudget()
{
    // 10 x 10 pixels take 400 bytes, so two such views fit in 1000
    PagePrescaler prescaler({ std::make_shared<ScaleStage>() }, 1, 1000);
    const PrescaleRequest current = sizedRequest(5, QSize(10, 10));
    const PrescaleRequest next = sizedRequest(6, QSize(10, 10));
    const PrescaleRequest afterNext = sizedRequest(7, QSize(10, 10));
    const PrescaleRequest previous = sizedRequest(4, QSize(10, 10));
    prescaler.schedule({ previous });
    QTRY_VERIFY(!prescaler.image(previous.key).isNull());

    prescaler.schedule({ current, next, afterNext, previous });

    QVERIFY(prescaler.image(previous.key).isNull());
    QTRY_VERIFY(!prescaler.image(current.key).isNull() && !prescaler.image(next.key).isNull());

    // the current view is kept even when it alone is over the budget
    const PrescaleRequest zoomedIn = sizedRequest(5, QSize(20, 20));
    prescaler.schedule({ zoomedIn, next });

    QVERIFY(prescaler.image(next.key).isNull());
    QTRY_VERIFY(!prescaler.image(zoomedIn.key).isNull());
}

void PagePrescaleTest::keepsOneViewScaledNowOutsideTheSchedule()
{
    PagePrescaler prescaler({ std::make_shared<ScaleStage>() }, 1);
    const PrescaleRequest scheduled = pageRequest(1);
    // the current page while the window is being resized
    const PrescaleRequest resized = pageRequest(2);
    const PrescaleRequest resizedAgain = pageRequest(3);
    prescaler.schedule({ scheduled });
    QTRY_VERIFY(!prescaler.image(scheduled.key).isNull());

    QVERIFY(!prescaler.scaleNow(resized).isNull());
    const QImage last = prescaler.scaleNow(resizedAgain);

    QVERIFY(!prescaler.image(scheduled.key).isNull());
    QVERIFY(prescaler.image(resized.key).isNull());
    QCOMPARE(prescaler.image(resizedAgain.key), last);

    prescaler.schedule({ resizedAgain });

    QVERIFY(prescaler.image(scheduled.key).isNull());
    QCOMPARE(prescaler.image(resizedAgain.key).cacheKey(), last.cacheKey());
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
    QVERIFY(entered.tryAcquire(1, kTimeoutMs));
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
    QVERIFY(entered.tryAcquire(1, kTimeoutMs));
    // page 1 only misses the slow stage
    QVERIFY(!prescaler.scaleNow(page1).isNull());
    prescaler.schedule({ page9, page1, page2, page3 });
    gate.release(10);

    QVERIFY(log.waitForEntries(4));
    QCOMPARE(log.snapshot().mid(0, 4), QStringList({ "fast:9", "fast:2", "fast:3", "slow:1" }));
}

void PagePrescaleTest::replacesImagesWithLaterStages()
{
    const auto first = std::make_shared<ColorStage>(QList<QColor> { Qt::red });
    PagePrescaler prescaler({ first, std::make_shared<ColorStage>(QList<QColor> { Qt::blue }) }, 1);
    QSignalSpy improved(&prescaler, &PagePrescaler::imageImproved);
    const PrescaleRequest page = pageRequest(1);

    prescaler.schedule({ page });

    QTRY_COMPARE(improved.count(), 2);
    QCOMPARE(improved.at(1).at(0).value<PrescaleKey>(), page.key);
    QCOMPARE(prescaler.image(page.key).pixelColor(0, 0), QColor(Qt::blue));
    // a view that has a better image does not go through the first stage again
    QCOMPARE(prescaler.scaleNow(page).pixelColor(0, 0), QColor(Qt::blue));
    QCOMPARE(first->calls.load(), 1);
}

void PagePrescaleTest::keepsThePreviousImageWhenAStageGivesNone()
{
    const auto none = std::make_shared<ColorStage>(QList<QColor> { QColor() });
    PagePrescaler prescaler({ std::make_shared<ColorStage>(QList<QColor> { Qt::red }), none }, 1);
    QSignalSpy improved(&prescaler, &PagePrescaler::imageImproved);
    const PrescaleRequest page = pageRequest(1);
    const PrescaleRequest barrier = pageRequest(2);

    prescaler.schedule({ page });
    // the second stage starts once the first one's result is delivered
    QTRY_VERIFY(none->entered.available() > 0);
    // on the one thread, the next view's work runs after the empty result is on its way back
    prescaler.schedule({ page, barrier });

    QTRY_VERIFY(!prescaler.image(barrier.key).isNull());
    QCOMPARE(prescaler.image(page.key).pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(improved.count(), 2);

    // and an empty first stage still lets the later ones run
    PagePrescaler emptyFirst({ std::make_shared<ColorStage>(QList<QColor> { QColor() }), std::make_shared<ColorStage>(QList<QColor> { Qt::blue }) }, 1);
    QSignalSpy improvedLater(&emptyFirst, &PagePrescaler::imageImproved);
    emptyFirst.schedule({ page });

    QTRY_VERIFY(!emptyFirst.image(page.key).isNull());
    QCOMPARE(emptyFirst.image(page.key).pixelColor(0, 0), QColor(Qt::blue));
    QCOMPARE(improvedLater.count(), 1);
}

void PagePrescaleTest::scalesNowAfterTheBackgroundGaveNoImage()
{
    // no image for the page in the background, red for the barrier and for scaleNow()
    const auto stage = std::make_shared<ColorStage>(QList<QColor> { QColor(), Qt::red });
    PagePrescaler prescaler({ stage }, 1);
    const PrescaleRequest page = pageRequest(1);
    const PrescaleRequest barrier = pageRequest(2);

    prescaler.schedule({ page });
    QVERIFY(stage->entered.tryAcquire(1, kTimeoutMs));
    prescaler.schedule({ page, barrier });
    // the barrier only starts once the empty result for the page has been posted
    QVERIFY(stage->entered.tryAcquire(1, kTimeoutMs));
    QCoreApplication::processEvents();

    QCOMPARE(prescaler.scaleNow(page).pixelColor(0, 0), QColor(Qt::red));
    QCOMPARE(prescaler.image(page.key).pixelColor(0, 0), QColor(Qt::red));
}

void PagePrescaleTest::cancelsTheBackgroundWorkScaleNowDoes()
{
    const auto stage = std::make_shared<CancellableStage>();
    PagePrescaler prescaler({ stage }, 1);
    QSignalSpy improved(&prescaler, &PagePrescaler::imageImproved);
    const PrescaleRequest page = pageRequest(1);
    prescaler.schedule({ page });
    QVERIFY(stage->entered.tryAcquire(1, kTimeoutMs));

    QCOMPARE(prescaler.scaleNow(page).pixelColor(0, 0), QColor(Qt::blue));

    QVERIFY(stage->sawCancel.tryAcquire(1, kTimeoutMs));
    QCoreApplication::processEvents();
    QCOMPARE(prescaler.image(page.key).pixelColor(0, 0), QColor(Qt::blue));
    QCOMPARE(improved.count(), 0);
}

void PagePrescaleTest::cancelsRunningWorkWhenDestroyed()
{
    const auto stage = std::make_shared<CancellableStage>();
    auto prescaler = std::make_unique<PagePrescaler>(QList<std::shared_ptr<const PrescaleStage>> { stage }, 1);
    prescaler->schedule({ pageRequest(1) });
    QVERIFY(stage->entered.tryAcquire(1, kTimeoutMs));

    // waits for the stage, which stops once it is cancelled
    prescaler.reset();

    QVERIFY(stage->sawCancel.tryAcquire(1, 0));
    // the result posted to the prescaler is dropped with it
    QCoreApplication::processEvents();
}

void PagePrescaleTest::dropsResultsFromBeforeAnInvalidation()
{
    const auto stage = std::make_shared<ColorStage>(QList<QColor> { Qt::red, Qt::green, Qt::blue });
    PagePrescaler prescaler({ stage }, 1);
    QSignalSpy improved(&prescaler, &PagePrescaler::imageImproved);
    const PrescaleRequest page = pageRequest(1);
    const PrescaleRequest barrier = pageRequest(2);

    prescaler.schedule({ page });
    QVERIFY(stage->entered.tryAcquire(1, kTimeoutMs));
    prescaler.schedule({ page, barrier });
    // on the one thread, the barrier starts once the red result is on its way back
    QVERIFY(stage->entered.tryAcquire(1, kTimeoutMs));
    prescaler.invalidate();
    prescaler.schedule({ page });

    QTRY_VERIFY(!prescaler.image(page.key).isNull());
    QCOMPARE(prescaler.image(page.key).pixelColor(0, 0), QColor(Qt::blue));
    QCOMPARE(improved.count(), 1);
}

void PagePrescaleTest::skipsStagesThatDecline()
{
    const auto declining = std::make_shared<DecliningStage>();
    PagePrescaler prescaler({ std::make_shared<ColorStage>(QList<QColor> { Qt::red }), declining,
                              std::make_shared<ColorStage>(QList<QColor> { Qt::blue }) },
                            1);
    QSignalSpy improved(&prescaler, &PagePrescaler::imageImproved);
    const PrescaleRequest page = doublePageRequest(1);

    prescaler.schedule({ page });

    QTRY_COMPARE(improved.count(), 2);
    QCOMPARE(prescaler.image(page.key).pixelColor(0, 0), QColor(Qt::blue));
    QCOMPARE(declining->calls.load(), 0);
    // the size of both pages together
    QCOMPARE(declining->askedSize, QSize(20, 20));
}

void PagePrescaleTest::drawsADoublePageOnceForAllStages()
{
    const auto first = std::make_shared<ColorStage>(QList<QColor> { Qt::red });
    const auto second = std::make_shared<ColorStage>(QList<QColor> { Qt::blue });
    PagePrescaler prescaler({ first, second }, 1);
    QSignalSpy improved(&prescaler, &PagePrescaler::imageImproved);
    const PrescaleRequest page = doublePageRequest(1);

    prescaler.schedule({ page });

    QTRY_COMPARE(improved.count(), 2);
    QCOMPARE(first->inputs().size(), 1);
    QCOMPARE(second->inputs(), first->inputs());
    QVERIFY(first->inputs().first() != page.images.first.cacheKey());

    // and once for every size while the window is resized
    const auto scale = std::make_shared<ColorStage>(QList<QColor> { Qt::red });
    PagePrescaler resized({ scale }, 1);
    PrescaleRequest shown = doublePageRequest(2);
    resized.scaleNow(shown);
    shown.key.pixelSize = QSize(12, 12);
    resized.scaleNow(shown);

    QCOMPARE(scale->inputs().size(), 2);
    QCOMPARE(scale->inputs().at(1), scale->inputs().at(0));
}

void PagePrescaleTest::limitsTheViewsAStageWorksOnAtOnce()
{
    const auto slow = std::make_shared<OneAtATimeStage>();
    PagePrescaler prescaler({ std::make_shared<ColorStage>(QList<QColor> { Qt::red }), slow }, 3);
    const PrescaleRequest first = pageRequest(1);
    const PrescaleRequest second = pageRequest(2);

    prescaler.schedule({ first, second });

    // both views have their first stage, and two threads are free for the second one's slow stage
    QTRY_VERIFY(!prescaler.image(first.key).isNull() && !prescaler.image(second.key).isNull());
    QVERIFY(slow->entered.tryAcquire(1, kTimeoutMs));
    QVERIFY(!slow->entered.tryAcquire(1, 200));
    slow->gate.release(2);

    QTRY_VERIFY(prescaler.image(first.key).pixelColor(0, 0) == QColor(Qt::green) && prescaler.image(second.key).pixelColor(0, 0) == QColor(Qt::green));
    QCOMPARE(slow->mostAtOnce.load(), 1);
}

QTEST_GUILESS_MAIN(PagePrescaleTest)

#include "main.moc"
