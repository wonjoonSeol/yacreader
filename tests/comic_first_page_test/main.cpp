#include "comic.h"

#include <QFile>
#include <QObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class ComicFirstPageTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void opensAtTheExpectedPage_data();
    void opensAtTheExpectedPage();

private:
    QTemporaryDir comicFolder;
};

namespace {

constexpr int numPages = 5;

}

void ComicFirstPageTest::initTestCase()
{
    // Bookmarks reads the reader's saved progress from the settings path
    QStandardPaths::setTestModeEnabled(true);

    QVERIFY(comicFolder.isValid());
    for (int page = 1; page <= numPages; ++page) {
        QFile file(comicFolder.filePath(QString("%1.jpg").arg(page)));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("page");
    }
}

void ComicFirstPageTest::opensAtTheExpectedPage_data()
{
    QTest::addColumn<int>("page");
    QTest::addColumn<bool>("openFinishedFromFirstPage");
    QTest::addColumn<int>("expectedPage");

    QTest::newRow("last page") << numPages - 1 << false << numPages - 1;
    QTest::newRow("last page, finished comics from the first page") << numPages - 1 << true << 0;
    QTest::newRow("second to last page, finished comics from the first page") << numPages - 2 << true << numPages - 2;
    QTest::newRow("middle page, finished comics from the first page") << 2 << true << 2;
    QTest::newRow("past the last page") << numPages << false << 0;
}

void ComicFirstPageTest::opensAtTheExpectedPage()
{
    QFETCH(int, page);
    QFETCH(bool, openFinishedFromFirstPage);
    QFETCH(int, expectedPage);

    FolderComic comic;
    comic.setOpenFinishedFromFirstPage(openFinishedFromFirstPage);
    QVERIFY(comic.load(comicFolder.path(), page));
    comic.process();

    QCOMPARE(static_cast<int>(comic.getIndex()), expectedPage);
}

QTEST_GUILESS_MAIN(ComicFirstPageTest)

#include "main.moc"
