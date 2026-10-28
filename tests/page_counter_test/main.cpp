#include "page_counter_trigger.h"
#include "pages_information.h"

#include <QTest>

using YACReader::PageSpread;

class PageCounterTest : public QObject
{
    Q_OBJECT

private slots:
    void pagesInformation_data();
    void pagesInformation();
    void pageTurned_data();
    void pageTurned();
    void resetCountsTheNextPageAsATurn();
};

void PageCounterTest::pagesInformation_data()
{
    QTest::addColumn<int>("page");
    QTest::addColumn<int>("numPages");
    QTest::addColumn<PageSpread>("spread");
    QTest::addColumn<QString>("expected");

    QTest::newRow("first page") << 0 << 24 << PageSpread::None << "1/24";
    QTest::newRow("single page") << 9 << 100 << PageSpread::None << "10/100";
    QTest::newRow("last page") << 99 << 100 << PageSpread::None << "100/100";
    QTest::newRow("spread") << 9 << 100 << PageSpread::LeftToRight << "10-11/100";
    QTest::newRow("manga spread") << 9 << 100 << PageSpread::RightToLeft << "11-10/100";
    QTest::newRow("spread ending on the last page") << 98 << 100 << PageSpread::LeftToRight << "99-100/100";
    QTest::newRow("spread asked for on the last page") << 99 << 100 << PageSpread::LeftToRight << "100/100";
    QTest::newRow("manga spread asked for on the last page") << 99 << 100 << PageSpread::RightToLeft << "100/100";
}

void PageCounterTest::pagesInformation()
{
    QFETCH(int, page);
    QFETCH(int, numPages);
    QFETCH(PageSpread, spread);
    QFETCH(QString, expected);

    QCOMPARE(YACReader::pagesInformation(page, numPages, spread), expected);
}

// Each row is a sequence of calls, one per change of the page on screen. continuousPages holds the
// reading page of the continuous scroll, or -1 outside continuous scroll.
void PageCounterTest::pageTurned_data()
{
    QTest::addColumn<QList<int>>("renderPages");
    QTest::addColumn<QList<int>>("continuousPages");
    QTest::addColumn<QList<bool>>("turned");

    QTest::newRow("opening a comic") << QList<int> { 0 } << QList<int> { -1 } << QList<bool> { true };
    QTest::newRow("next page") << QList<int> { 0, 1 } << QList<int> { -1, -1 } << QList<bool> { true, true };
    // Render is only synced with the scroll before its own page turns
    QTest::newRow("continuous scroll onto the next page")
            << QList<int> { 0, 0 } << QList<int> { 0, 1 } << QList<bool> { true, true };
    QTest::newRow("continuous scroll within the page")
            << QList<int> { 4, 4 } << QList<int> { 4, 4 } << QList<bool> { true, false };
    QTest::newRow("continuous scroll, Next after scrolling")
            << QList<int> { 0, 0, 20 } << QList<int> { 0, 19, 20 } << QList<bool> { true, true, true };
    // Render's page already equals the target, so Render itself reports no page change
    QTest::newRow("continuous scroll, first page after scrolling down")
            << QList<int> { 0, 0, 0 } << QList<int> { 0, 19, 0 } << QList<bool> { true, true, true };
    // Render catches up with the reading page, which stays on screen
    QTest::newRow("leaving continuous scroll")
            << QList<int> { 0, 0, 19 } << QList<int> { 0, 19, -1 } << QList<bool> { true, true, false };
}

void PageCounterTest::pageTurned()
{
    QFETCH(QList<int>, renderPages);
    QFETCH(QList<int>, continuousPages);
    QFETCH(QList<bool>, turned);
    QCOMPARE(continuousPages.size(), renderPages.size());
    QCOMPARE(turned.size(), renderPages.size());

    PageCounterTrigger trigger;
    for (int i = 0; i < renderPages.size(); ++i) {
        const std::optional<int> continuousPage = continuousPages.at(i) >= 0 ? std::optional<int>(continuousPages.at(i)) : std::nullopt;
        QVERIFY2(trigger.pageTurned(renderPages.at(i), continuousPage) == turned.at(i), qPrintable(QString("call %1").arg(i + 1)));
    }
}

void PageCounterTest::resetCountsTheNextPageAsATurn()
{
    PageCounterTrigger trigger;
    QVERIFY(trigger.pageTurned(3, std::nullopt));

    trigger.reset();

    QVERIFY(trigger.pageTurned(3, std::nullopt));
}

QTEST_GUILESS_MAIN(PageCounterTest)

#include "main.moc"
