#include "continuous_view_model.h"

#include <QObject>
#include <QSize>
#include <QTest>

#include <limits>

class ContinuousViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void usesCenterPageForNormalProgress();
    void doesNotReportLastPageWhenLessThanHalfIsVisible();
    void reportsLastPageWhenAtLeastHalfIsVisible();
    void fitsPagesToTheViewportWidthByDefault();
    void fitsEachPageToTheViewportHeight();
    void fitsPagesWiderThanTheViewportToItsWidthInHeightMode();
    void keepsTheReadingPositionWhenThePageFitChanges();
    void refitsPagesToANewViewportHeightOnlyInHeightMode();
};

void ContinuousViewModelTest::usesCenterPageForNormalProgress()
{
    ContinuousViewModel model(std::numeric_limits<int>::max());
    model.setNumPages(4);
    model.setViewportSize(100, 250);
    model.setPageNaturalSize(0, QSize(100, 100));
    model.setPageNaturalSize(1, QSize(100, 100));
    model.setPageNaturalSize(2, QSize(100, 100));
    model.setPageNaturalSize(3, QSize(100, 25));

    model.setScrollYFromUser(49);

    QCOMPARE(model.centerPage(), 1);
    QCOMPARE(model.readingProgressPage(), 1);
}

// Layout: pages 0-2 at y=0,100,200 (100px each), page 3 at y=300 (25px).
// lastPageMidY = 300 + 12 = 312. viewportBottomY = scrollY + 249.
// Threshold scroll: 312 - 249 = 63.

void ContinuousViewModelTest::doesNotReportLastPageWhenLessThanHalfIsVisible()
{
    ContinuousViewModel model(std::numeric_limits<int>::max());
    model.setNumPages(4);
    model.setViewportSize(100, 250);
    model.setPageNaturalSize(0, QSize(100, 100));
    model.setPageNaturalSize(1, QSize(100, 100));
    model.setPageNaturalSize(2, QSize(100, 100));
    model.setPageNaturalSize(3, QSize(100, 25));

    model.setScrollYFromUser(62); // viewportBottomY=311, one pixel short of midpoint

    QCOMPARE(model.readingProgressPage(), model.centerPage());
}

void ContinuousViewModelTest::reportsLastPageWhenAtLeastHalfIsVisible()
{
    ContinuousViewModel model(std::numeric_limits<int>::max());
    model.setNumPages(4);
    model.setViewportSize(100, 250);
    model.setPageNaturalSize(0, QSize(100, 100));
    model.setPageNaturalSize(1, QSize(100, 100));
    model.setPageNaturalSize(2, QSize(100, 100));
    model.setPageNaturalSize(3, QSize(100, 25));

    model.setScrollYFromUser(63); // viewportBottomY=312, exactly at midpoint

    QCOMPARE(model.readingProgressPage(), 3);
}

void ContinuousViewModelTest::fitsPagesToTheViewportWidthByDefault()
{
    ContinuousViewModel model(std::numeric_limits<int>::max());
    model.setNumPages(1);
    model.setViewportSize(1000, 500);
    model.setPageNaturalSize(0, QSize(400, 800));

    QCOMPARE(model.scaledPageSize(0), QSize(1000, 2000));
}

void ContinuousViewModelTest::fitsEachPageToTheViewportHeight()
{
    ContinuousViewModel model(std::numeric_limits<int>::max());
    model.setNumPages(3);
    model.setViewportSize(1000, 500);
    model.setPageFit(ContinuousViewModel::PageFit::Height);
    model.setPageNaturalSize(0, QSize(400, 800));
    model.setPageNaturalSize(1, QSize(600, 800));
    model.setPageNaturalSize(2, QSize(800, 1600));

    QCOMPARE(model.scaledPageSize(0), QSize(250, 500));
    QCOMPARE(model.scaledPageSize(1), QSize(375, 500));
    QCOMPARE(model.scaledPageSize(2), QSize(250, 500));
    QCOMPARE(model.yPositionForPage(2), 1000);
}

void ContinuousViewModelTest::fitsPagesWiderThanTheViewportToItsWidthInHeightMode()
{
    ContinuousViewModel model(std::numeric_limits<int>::max());
    model.setNumPages(2);
    model.setViewportSize(1000, 500);
    model.setPageFit(ContinuousViewModel::PageFit::Height);
    model.setPageNaturalSize(0, QSize(400, 800));
    model.setPageNaturalSize(1, QSize(3000, 1000));

    QCOMPARE(model.scaledPageSize(0), QSize(250, 500));
    QCOMPARE(model.scaledPageSize(1), QSize(1000, 333));
    QCOMPARE(model.yPositionForPage(1), 500);
}

void ContinuousViewModelTest::keepsTheReadingPositionWhenThePageFitChanges()
{
    ContinuousViewModel model(std::numeric_limits<int>::max());
    model.setNumPages(4);
    model.setViewportSize(1000, 500);
    for (int page = 0; page < 4; ++page) {
        model.setPageNaturalSize(page, QSize(400, 800));
    }
    model.setScrollYFromUser(4750); // the viewport center is the middle of page 2
    QCOMPARE(model.centerPage(), 2);

    model.setPageFit(ContinuousViewModel::PageFit::Height);

    QCOMPARE(model.scaledPageSize(0), QSize(250, 500));
    QCOMPARE(model.centerPage(), 2);

    model.setPageFit(ContinuousViewModel::PageFit::Width);

    QCOMPARE(model.scaledPageSize(0), QSize(1000, 2000));
    QCOMPARE(model.centerPage(), 2);
}

void ContinuousViewModelTest::refitsPagesToANewViewportHeightOnlyInHeightMode()
{
    ContinuousViewModel heightModel(std::numeric_limits<int>::max());
    heightModel.setNumPages(1);
    heightModel.setViewportSize(1000, 500);
    heightModel.setPageFit(ContinuousViewModel::PageFit::Height);
    heightModel.setPageNaturalSize(0, QSize(400, 800));

    ContinuousViewModel widthModel(std::numeric_limits<int>::max());
    widthModel.setNumPages(1);
    widthModel.setViewportSize(1000, 500);
    widthModel.setPageNaturalSize(0, QSize(400, 800));

    heightModel.setViewportSize(1000, 600);
    widthModel.setViewportSize(1000, 600);

    QCOMPARE(heightModel.scaledPageSize(0), QSize(300, 600));
    QCOMPARE(widthModel.scaledPageSize(0), QSize(1000, 2000));
}

QTEST_GUILESS_MAIN(ContinuousViewModelTest)

#include "main.moc"
