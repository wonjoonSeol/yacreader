#include "comic.h"
#include "yacreader_global.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using YACReader::OpenComicsAt;

class ComicStartPageTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void startPage_data();
    void startPage();
    void saveBookmarksRecordsTheLastReadPage();
    void saveBookmarksIgnoresPagesOutsideTheComic();

private:
    QTemporaryDir comicFolder;
};

namespace {

constexpr int pageCount = 5;
constexpr int lastPage = pageCount - 1;
constexpr int savedProgress = -1;

}

void ComicStartPageTest::initTestCase()
{
    // keeps the bookmarks file that Comic reads and writes out of the user's data
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(QDir().mkpath(YACReader::getSettingsPath()));

    QVERIFY(comicFolder.isValid());
    for (int page = 1; page <= pageCount; ++page) {
        QFile file(comicFolder.filePath(QStringLiteral("%1.png").arg(page)));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("page"), qint64(4));
    }
}

void ComicStartPageTest::cleanupTestCase()
{
    QFile::remove(YACReader::getSettingsPath() + "/bookmarks.yacr");
}

void ComicStartPageTest::startPage_data()
{
    QTest::addColumn<int>("requestedPage");
    QTest::addColumn<int>("savedPage");
    QTest::addColumn<OpenComicsAt>("openComicsAt");
    QTest::addColumn<int>("expectedPage");

    QTest::newRow("saved on the last page, last page read") << savedProgress << lastPage << OpenComicsAt::LastPageRead << lastPage;
    QTest::newRow("saved on the last page, first page if finished") << savedProgress << lastPage << OpenComicsAt::FirstPageIfFinished << 0;
    QTest::newRow("saved on the second to last page, first page if finished") << savedProgress << lastPage - 1 << OpenComicsAt::FirstPageIfFinished << lastPage - 1;
    QTest::newRow("saved on a middle page, always the first page") << savedProgress << 2 << OpenComicsAt::FirstPage << 0;
    QTest::newRow("saved before the first page") << savedProgress << -1 << OpenComicsAt::LastPageRead << 0;
    QTest::newRow("requested last page, first page if finished") << lastPage << 2 << OpenComicsAt::FirstPageIfFinished << lastPage;
    QTest::newRow("requested middle page, always the first page") << 2 << 3 << OpenComicsAt::FirstPage << 2;
    QTest::newRow("requested page past the end") << pageCount << 2 << OpenComicsAt::LastPageRead << 0;
}

void ComicStartPageTest::startPage()
{
    QFETCH(int, requestedPage);
    QFETCH(int, savedPage);
    QFETCH(OpenComicsAt, openComicsAt);
    QFETCH(int, expectedPage);

    FolderComic comic;
    comic.setOpenComicsAt(openComicsAt);
    comic.load(comicFolder.path(), requestedPage);
    comic.bm->setLastPage(savedPage, QImage());
    QSignalSpy openAt(&comic, &Comic::openAt);

    comic.process();

    QVERIFY(!comic.hasBeenAnErrorOpening());
    QCOMPARE(comic.numPages(), static_cast<unsigned int>(pageCount));
    QCOMPARE(openAt.count(), 1);
    QCOMPARE(openAt.at(0).at(0).toInt(), expectedPage);
}

void ComicStartPageTest::saveBookmarksRecordsTheLastReadPage()
{
    FolderComic comic;
    comic.load(comicFolder.path(), 1);
    comic.process();
    QCOMPARE(comic.getIndex(), 1u);

    comic.saveBookmarks(lastPage);

    QCOMPARE(comic.bm->getLastPage(), lastPage);
}

void ComicStartPageTest::saveBookmarksIgnoresPagesOutsideTheComic()
{
    FolderComic comic;
    comic.load(comicFolder.path(), 1);
    comic.process();
    comic.saveBookmarks(2);

    comic.saveBookmarks(pageCount);
    comic.saveBookmarks(-1);

    QCOMPARE(comic.bm->getLastPage(), 2);
}

QTEST_GUILESS_MAIN(ComicStartPageTest)

#include "main.moc"
