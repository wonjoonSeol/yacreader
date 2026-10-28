#include "comic.h"
#include "comic_db.h"
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
    void init();
    void cleanupTestCase();
    void choosesTheStartPage_data();
    void choosesTheStartPage();
    void choosesTheStartPageOfLibraryComics_data();
    void choosesTheStartPageOfLibraryComics();
    void opensLibraryFolderComicsAtTheSavedPage_data();
    void opensLibraryFolderComicsAtTheSavedPage();
    void reportsALibraryFolderComicThatIsGone();
    void reopenedComicStartsFromTheSavedProgress_data();
    void reopenedComicStartsFromTheSavedProgress();
    void saveBookmarksRecordsTheLastReadPageAndItsImage();
    void saveBookmarksKeepsTheLastReadPageForPagesOutsideTheComic_data();
    void saveBookmarksKeepsTheLastReadPageForPagesOutsideTheComic();

private:
    QTemporaryDir comicFolder;
};

namespace {

constexpr int pageCount = 5;
constexpr int lastPage = pageCount - 1;
constexpr int atSavedProgress = -1;

QSize pageSize(int page)
{
    return QSize(10 * (page + 1), 20 * (page + 1));
}

QString bookmarksPath()
{
    return YACReader::getSettingsPath() + "/bookmarks.yacr";
}

// the page reported to Render, or -1 when it was not reported exactly once
int startPageOf(Comic &comic)
{
    QSignalSpy openAt(&comic, &Comic::openAt);
    comic.process();
    return openAt.count() == 1 ? openAt.at(0).at(0).toInt() : -1;
}

class LibraryComicProbe : public FileComic
{
public:
    using Comic::resolveStartPage;
    void setPageCount(int count) { _pages.resize(count); }
    int startPage() const { return _firstPage; }
};

}

void ComicStartPageTest::initTestCase()
{
    // keeps the bookmarks file that Comic reads and writes out of the user's data
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(QDir().mkpath(YACReader::getSettingsPath()));

    QVERIFY(comicFolder.isValid());
    for (int page = 0; page < pageCount; ++page) {
        QImage image(pageSize(page), QImage::Format_RGB32);
        image.fill(Qt::white);
        QVERIFY(image.save(comicFolder.filePath(QStringLiteral("%1.png").arg(page + 1))));
    }
}

void ComicStartPageTest::init()
{
    QFile::remove(bookmarksPath());
}

void ComicStartPageTest::cleanupTestCase()
{
    QFile::remove(bookmarksPath());
}

void ComicStartPageTest::choosesTheStartPage_data()
{
    QTest::addColumn<int>("requestedPage");
    QTest::addColumn<int>("savedPage");
    QTest::addColumn<OpenComicsAt>("openComicsAt");
    QTest::addColumn<int>("expectedPage");

    QTest::newRow("saved on the last page, last read page") << atSavedProgress << lastPage << OpenComicsAt::LastReadPage << lastPage;
    QTest::newRow("saved on the last page, first page if finished") << atSavedProgress << lastPage << OpenComicsAt::FirstPageIfFinished << 0;
    QTest::newRow("saved on the second to last page, first page if finished") << atSavedProgress << lastPage - 1 << OpenComicsAt::FirstPageIfFinished << lastPage - 1;
    QTest::newRow("saved on a middle page, last read page") << atSavedProgress << 2 << OpenComicsAt::LastReadPage << 2;
    QTest::newRow("saved on a middle page, first page") << atSavedProgress << 2 << OpenComicsAt::FirstPage << 0;
    QTest::newRow("saved past the last page, last read page") << atSavedProgress << pageCount << OpenComicsAt::LastReadPage << 0;
    QTest::newRow("saved on the last page, unknown setting") << atSavedProgress << lastPage << static_cast<OpenComicsAt>(7) << lastPage;
    QTest::newRow("requested last page, first page if finished") << lastPage << 2 << OpenComicsAt::FirstPageIfFinished << lastPage;
    QTest::newRow("requested middle page, first page") << 2 << 3 << OpenComicsAt::FirstPage << 2;
    QTest::newRow("requested page past the end") << pageCount << 2 << OpenComicsAt::LastReadPage << 0;
}

void ComicStartPageTest::choosesTheStartPage()
{
    QFETCH(int, requestedPage);
    QFETCH(int, savedPage);
    QFETCH(OpenComicsAt, openComicsAt);
    QFETCH(int, expectedPage);

    FolderComic comic;
    comic.setOpenComicsAt(openComicsAt);
    QVERIFY(comic.load(comicFolder.path(), requestedPage));
    comic.bm->setLastPage(savedPage, QImage());

    QCOMPARE(startPageOf(comic), expectedPage);
    QVERIFY(!comic.hasBeenAnErrorOpening());
    QCOMPARE(comic.numPages(), static_cast<unsigned int>(pageCount));
}

void ComicStartPageTest::choosesTheStartPageOfLibraryComics_data()
{
    QTest::addColumn<int>("currentPage");
    QTest::addColumn<OpenComicsAt>("openComicsAt");
    QTest::addColumn<int>("expectedPage");

    QTest::newRow("finished, last read page") << pageCount << OpenComicsAt::LastReadPage << lastPage;
    QTest::newRow("finished, first page if finished") << pageCount << OpenComicsAt::FirstPageIfFinished << 0;
    QTest::newRow("on the second to last page, first page if finished") << pageCount - 1 << OpenComicsAt::FirstPageIfFinished << lastPage - 1;
    QTest::newRow("on a middle page, first page") << 3 << OpenComicsAt::FirstPage << 0;
    QTest::newRow("removed from Continue reading") << 0 << OpenComicsAt::LastReadPage << 0;
}

void ComicStartPageTest::choosesTheStartPageOfLibraryComics()
{
    QFETCH(int, currentPage);
    QFETCH(OpenComicsAt, openComicsAt);
    QFETCH(int, expectedPage);

    ComicDB comicDB;
    comicDB.info.currentPage = currentPage;

    LibraryComicProbe comic;
    comic.setOpenComicsAt(openComicsAt);
    QVERIFY(comic.load(comicFolder.filePath("1.png"), comicDB));
    comic.setPageCount(pageCount);
    comic.resolveStartPage();

    QCOMPARE(comic.startPage(), expectedPage);
}

void ComicStartPageTest::opensLibraryFolderComicsAtTheSavedPage_data()
{
    QTest::addColumn<int>("currentPage");
    QTest::addColumn<OpenComicsAt>("openComicsAt");
    QTest::addColumn<int>("expectedPage");

    QTest::newRow("on a middle page") << 3 << OpenComicsAt::LastReadPage << 2;
    QTest::newRow("never opened") << 0 << OpenComicsAt::LastReadPage << 0;
    QTest::newRow("finished, first page if finished") << pageCount << OpenComicsAt::FirstPageIfFinished << 0;
    QTest::newRow("on a middle page, first page if finished") << 3 << OpenComicsAt::FirstPageIfFinished << 2;
    QTest::newRow("on a middle page, first page") << 3 << OpenComicsAt::FirstPage << 0;
}

void ComicStartPageTest::opensLibraryFolderComicsAtTheSavedPage()
{
    QFETCH(int, currentPage);
    QFETCH(OpenComicsAt, openComicsAt);
    QFETCH(int, expectedPage);

    ComicDB comicDB;
    comicDB.info.currentPage = currentPage;

    FolderComic comic;
    comic.setOpenComicsAt(openComicsAt);
    QVERIFY(comic.load(comicFolder.path(), comicDB));

    QCOMPARE(startPageOf(comic), expectedPage);
    QVERIFY(!comic.hasBeenAnErrorOpening());
    QCOMPARE(comic.numPages(), static_cast<unsigned int>(pageCount));
}

void ComicStartPageTest::reportsALibraryFolderComicThatIsGone()
{
    // like a comic file, a folder that is no longer there is an error at once
    FolderComic comic;
    QVERIFY(!comic.load(comicFolder.filePath("Missing"), ComicDB()));
    QVERIFY(comic.hasBeenAnErrorOpening());
}

void ComicStartPageTest::reopenedComicStartsFromTheSavedProgress_data()
{
    QTest::addColumn<OpenComicsAt>("openComicsAt");
    QTest::addColumn<int>("expectedPage");

    QTest::newRow("last read page") << OpenComicsAt::LastReadPage << lastPage;
    QTest::newRow("first page if finished") << OpenComicsAt::FirstPageIfFinished << 0;
}

void ComicStartPageTest::reopenedComicStartsFromTheSavedProgress()
{
    QFETCH(OpenComicsAt, openComicsAt);
    QFETCH(int, expectedPage);

    {
        FolderComic comic;
        QVERIFY(comic.load(comicFolder.path()));
        QCOMPARE(startPageOf(comic), 0);
        comic.saveBookmarks(lastPage);
    }

    FolderComic reopened;
    reopened.setOpenComicsAt(openComicsAt);
    QVERIFY(reopened.load(comicFolder.path()));

    QCOMPARE(startPageOf(reopened), expectedPage);
    QCOMPARE(reopened.numPages(), static_cast<unsigned int>(pageCount));
}

void ComicStartPageTest::saveBookmarksRecordsTheLastReadPageAndItsImage()
{
    FolderComic comic;
    QVERIFY(comic.load(comicFolder.path(), 1));
    QCOMPARE(startPageOf(comic), 1);

    comic.saveBookmarks(lastPage);

    QCOMPARE(comic.bm->getLastPage(), lastPage);
    QCOMPARE(comic.bm->getLastPagePixmap().size(), pageSize(lastPage));
}

void ComicStartPageTest::saveBookmarksKeepsTheLastReadPageForPagesOutsideTheComic_data()
{
    QTest::addColumn<int>("page");

    QTest::newRow("past the last page") << pageCount;
    QTest::newRow("before the first page") << -1;
}

void ComicStartPageTest::saveBookmarksKeepsTheLastReadPageForPagesOutsideTheComic()
{
    QFETCH(int, page);

    FolderComic comic;
    QVERIFY(comic.load(comicFolder.path(), 1));
    QCOMPARE(startPageOf(comic), 1);
    comic.saveBookmarks(2);

    comic.saveBookmarks(page);

    QCOMPARE(comic.bm->getLastPage(), 2);
}

QTEST_GUILESS_MAIN(ComicStartPageTest)

#include "main.moc"
