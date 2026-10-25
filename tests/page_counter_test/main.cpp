#include "page_counter.h"

#include <QTest>

class PageCounterTest : public QObject
{
    Q_OBJECT

private slots:
    void pagesInformation_data();
    void pagesInformation();
    void pageCounterTop_data();
    void pageCounterTop();
};

void PageCounterTest::pagesInformation_data()
{
    QTest::addColumn<int>("page");
    QTest::addColumn<int>("numPages");
    QTest::addColumn<bool>("spread");
    QTest::addColumn<bool>("manga");
    QTest::addColumn<QString>("expected");

    QTest::newRow("first page") << 0 << 24 << false << false << "1/24";
    QTest::newRow("single page") << 9 << 100 << false << false << "10/100";
    QTest::newRow("last page") << 99 << 100 << false << false << "100/100";
    QTest::newRow("spread") << 9 << 100 << true << false << "10-11/100";
    QTest::newRow("manga spread") << 9 << 100 << true << true << "11-10/100";
    QTest::newRow("manga single page") << 9 << 100 << false << true << "10/100";
    QTest::newRow("spread ending on the last page") << 98 << 100 << true << false << "99-100/100";
    QTest::newRow("spread asked for on the last page") << 99 << 100 << true << false << "100/100";
    QTest::newRow("manga spread asked for on the last page") << 99 << 100 << true << true << "100/100";
}

void PageCounterTest::pagesInformation()
{
    QFETCH(int, page);
    QFETCH(int, numPages);
    QFETCH(bool, spread);
    QFETCH(bool, manga);
    QFETCH(QString, expected);

    QCOMPARE(YACReader::pagesInformation(page, numPages, spread, manga), expected);
}

void PageCounterTest::pageCounterTop_data()
{
    QTest::addColumn<QList<QRect>>("toolBarAreas");
    QTest::addColumn<int>("expected");

    QTest::newRow("no toolbar") << QList<QRect> { } << 0;
    QTest::newRow("toolbar above the page") << QList<QRect> { QRect(0, -40, 800, 40) } << 0;
    QTest::newRow("toolbar over the top of the page") << QList<QRect> { QRect(0, 0, 800, 40) } << 40;
    QTest::newRow("toolbar lower on the page") << QList<QRect> { QRect(0, 500, 800, 40) } << 0;
    QTest::newRow("toolbar right of the corner") << QList<QRect> { QRect(100, 0, 700, 40) } << 0;
    QTest::newRow("toolbar above and toolbar over the page") << QList<QRect> { QRect(0, -40, 800, 40), QRect(0, 0, 800, 30) } << 30;
    QTest::newRow("taller toolbar first") << QList<QRect> { QRect(0, 0, 800, 60), QRect(0, 0, 800, 40) } << 60;
}

void PageCounterTest::pageCounterTop()
{
    QFETCH(QList<QRect>, toolBarAreas);
    QFETCH(int, expected);

    QCOMPARE(YACReader::pageCounterTop(toolBarAreas), expected);
}

QTEST_GUILESS_MAIN(PageCounterTest)

#include "main.moc"
