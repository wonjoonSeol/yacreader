#include "comic.h"
#include "comics_remover.h"
#include "data_base_management.h"
#include "library_creator.h"
#include "yacreader_global.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QSet>
#include <QSettings>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringListModel>
#include <QTemporaryDir>
#include <QTest>

using YACReader::LibraryPaths;

class LibraryImageFoldersTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void tellsComicFoldersApart_data();
    void tellsComicFoldersApart();
    void importsFoldersOfImagesAsComics();
    void updateFollowsTheFoldersOfImages();
    void deletesFolderComicsOnlyWhenAsked();
    void deletesFolderComicsForGoodWhenToldTo();
};

namespace {

struct ComicRow {
    qulonglong id;
    int numPages;
    QString hash;
    QString coverSize;
    QString title;
};

QSize pageSize(int page)
{
    return QSize(100 + 10 * page, 150 + 10 * page);
}

// pages with the same number look the same in comics made with the same seed, seeds go up to 14
bool writePages(const QString &folder, int count, int seed, const QString &namePrefix = QString())
{
    if (!QDir().mkpath(folder)) {
        return false;
    }
    for (int page = 1; page <= count; ++page) {
        QImage image(pageSize(page), QImage::Format_RGB32);
        image.fill(QColor::fromHsv(seed * 25, 60 + page * 15, 220));
        if (!image.save(QDir(folder).filePath(QStringLiteral("%1%2.png").arg(namePrefix, QString::number(page))))) {
            return false;
        }
    }
    return true;
}

bool writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

bool runUntil(LibraryCreator &creator, void (LibraryCreator::*finishedSignal)())
{
    QSignalSpy finished(&creator, finishedSignal);
    creator.start();
    return creator.wait() && finished.count() == 1;
}

bool createLibrary(const QString &source, QSettings &settings)
{
    LibraryCreator creator(&settings);
    creator.createLibrary(source, LibraryPaths::libraryDataPath(source));
    return runUntil(creator, &LibraryCreator::created);
}

bool updateLibrary(const QString &source, QSettings &settings)
{
    LibraryCreator creator(&settings);
    creator.updateLibrary(source, LibraryPaths::libraryDataPath(source));
    return runUntil(creator, &LibraryCreator::updated);
}

QMap<QString, ComicRow> comicsByPath(const QString &source)
{
    QMap<QString, ComicRow> comics;
    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(LibraryPaths::libraryDataPath(source));
        connectionName = db.connectionName();
        QSqlQuery query("SELECT c.id, c.path, ci.numPages, ci.hash, ci.originalCoverSize, ci.title "
                        "FROM comic c INNER JOIN comic_info ci ON c.comicInfoId = ci.id",
                        db);
        while (query.next()) {
            comics.insert(query.value(1).toString(), { query.value(0).toULongLong(), query.value(2).toInt(), query.value(3).toString(), query.value(4).toString(), query.value(5).toString() });
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return comics;
}

QMap<QString, qulonglong> folderIdsByPath(const QString &source)
{
    QMap<QString, qulonglong> folders;
    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(LibraryPaths::libraryDataPath(source));
        connectionName = db.connectionName();
        QSqlQuery query("SELECT path, id FROM folder WHERE id > 1", db);
        while (query.next()) {
            folders.insert(query.value(0).toString(), query.value(1).toULongLong());
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return folders;
}

QMap<QString, int> pageCounts(const QMap<QString, ComicRow> &comics)
{
    QMap<QString, int> counts;
    for (auto comic = comics.cbegin(); comic != comics.cend(); ++comic) {
        counts.insert(comic.key(), comic->numPages);
    }
    return counts;
}

void verifyCoversAndHashes(const QString &source, const QMap<QString, ComicRow> &comics)
{
    QSet<QString> hashes;
    for (const ComicRow &comic : comics) {
        hashes.insert(comic.hash);
        QVERIFY2(QFile::exists(LibraryPaths::coverPath(source, comic.hash)), qPrintable(comic.hash));
        QCOMPARE(comic.coverSize, QStringLiteral("%1x%2").arg(pageSize(1).width()).arg(pageSize(1).height()));
    }
    QCOMPARE(hashes.size(), comics.size());
}

struct PageFolder {
    QString path;
    int pageCount;
    int seed;
    QString namePrefix;
};

bool buildLibrary(const QString &source)
{
    // Chapter 10 starts like Chapter 1 and Reprint has the pages of Chapter 1 under other
    // names, so only their page lists tell these three apart
    const QList<PageFolder> pageFolders {
        { "/Solo", 3, 1 },
        { "/Series/Chapter 1", 2, 2 },
        { "/Reprint", 2, 2, "p" },
        { "/Series/Chapter 2", 4, 3 },
        { "/Series/Chapter 10", 6, 2 },
        { "/Mixed", 2, 4 },
        { "/Mixed/Extras", 5, 5 },
        { "/With archive", 1, 6 },
        { "/.hidden", 2, 7 },
        { "/Vol 2/Part", 1, 10 },
        { "/Vol 10/Part", 2, 11 },
    };
    const QMap<QString, QByteArray> files {
        { "/Solo/notes.txt", "notes" },
        { "/Solo/ComicInfo.xml", "<ComicInfo><Title>Solo Title</Title></ComicInfo>" },
        { "/Notes/readme.txt", "readme" },
        { "/With archive/Book.cbz", QByteArray(4096, 'x') },
    };

    for (const PageFolder &folder : pageFolders) {
        if (!writePages(source + folder.path, folder.pageCount, folder.seed, folder.namePrefix)) {
            return false;
        }
    }
    for (auto file = files.cbegin(); file != files.cend(); ++file) {
        if (!writeFile(source + file.key(), file.value())) {
            return false;
        }
    }
    return true;
}

}

void LibraryImageFoldersTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void LibraryImageFoldersTest::tellsComicFoldersApart_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QStringList>("files");
    QTest::addColumn<bool>("isComic");

    QTest::newRow("images") << "Chapter" << QStringList { "1.png", "2.jpg" } << true;
    QTest::newRow("images and other files") << "Chapter" << QStringList { "1.png", "notes.txt", "ComicInfo.xml" } << true;
    QTest::newRow("no images") << "Chapter" << QStringList { "notes.txt" } << false;
    QTest::newRow("empty") << "Chapter" << QStringList { } << false;
    QTest::newRow("images and a subfolder") << "Chapter" << QStringList { "1.png", "Extras/1.png" } << false;
    QTest::newRow("images and an archive") << "Chapter" << QStringList { "1.png", "Book.cbz" } << false;
    QTest::newRow("images and a PDF") << "Chapter" << QStringList { "1.png", "Book.pdf" } << false;
    QTest::newRow("dot folder") << ".thumbnails" << QStringList { "1.png" } << false;
}

void LibraryImageFoldersTest::tellsComicFoldersApart()
{
    QFETCH(QString, name);
    QFETCH(QStringList, files);
    QFETCH(bool, isComic);

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto folder = root.filePath(name);
    QVERIFY(QDir().mkpath(folder));
    for (const auto &file : files) {
        QVERIFY(writeFile(QDir(folder).filePath(file), "data"));
    }

    QCOMPARE(Comic::folderIsComic(folder), isComic);
}

void LibraryImageFoldersTest::importsFoldersOfImagesAsComics()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto source = root.path();
    QVERIFY(buildLibrary(source));
    QSettings settings(root.filePath("settings.ini"), QSettings::IniFormat);
    settings.setValue(IMPORT_COMIC_INFO_XML_METADATA, true);

    QVERIFY(createLibrary(source, settings));

    const auto comics = comicsByPath(source);
    const QMap<QString, int> expected {
        { "/Solo", 3 },
        { "/Series/Chapter 1", 2 },
        { "/Series/Chapter 2", 4 },
        { "/Series/Chapter 10", 6 },
        { "/Reprint", 2 },
        { "/Mixed/Extras", 5 },
        { "/Vol 2/Part", 1 },
        { "/Vol 10/Part", 2 },
    };
    QCOMPARE(pageCounts(comics), expected);
    QCOMPARE(folderIdsByPath(source).keys(), QStringList({ "/Mixed", "/Series", "/Vol 10", "/Vol 2" }));
    verifyCoversAndHashes(source, comics);
    QCOMPARE(comics.value("/Solo").title, QStringLiteral("Solo Title"));
}

void LibraryImageFoldersTest::updateFollowsTheFoldersOfImages()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto source = root.path();
    QVERIFY(buildLibrary(source));
    QSettings settings(root.filePath("settings.ini"), QSettings::IniFormat);
    QVERIFY(createLibrary(source, settings));
    const auto comicsBefore = comicsByPath(source);
    const auto foldersBefore = folderIdsByPath(source);

    QVERIFY(writePages(source + "/Series/Chapter 1", 3, 2));
    QVERIFY(QDir(source + "/Series/Chapter 2").removeRecursively());
    QVERIFY(writePages(source + "/Series/Chapter 3", 5, 8));
    QVERIFY(writePages(source + "/Solo/Bonus", 1, 9));
    QVERIFY(QDir(source + "/Mixed/Extras").removeRecursively());
    QVERIFY(writePages(source + "/Vol 3/Part", 3, 12));

    QVERIFY(updateLibrary(source, settings));

    const auto comics = comicsByPath(source);
    const auto folders = folderIdsByPath(source);
    const QMap<QString, int> expected {
        { "/Solo/Bonus", 1 },
        { "/Series/Chapter 1", 3 },
        { "/Series/Chapter 3", 5 },
        { "/Series/Chapter 10", 6 },
        { "/Reprint", 2 },
        { "/Mixed", 2 },
        { "/Vol 2/Part", 1 },
        { "/Vol 3/Part", 3 },
        { "/Vol 10/Part", 2 },
    };
    QCOMPARE(pageCounts(comics), expected);
    QCOMPARE(folders.keys(), QStringList({ "/Series", "/Solo", "/Vol 10", "/Vol 2", "/Vol 3" }));
    verifyCoversAndHashes(source, comics);
    QVERIFY(comics.value("/Series/Chapter 1").hash != comicsBefore.value("/Series/Chapter 1").hash);
    QCOMPARE(comics.value("/Series/Chapter 10").hash, comicsBefore.value("/Series/Chapter 10").hash);
    // what the update did not change keeps its row, and with it its reading lists and labels
    QCOMPARE(comics.value("/Series/Chapter 10").id, comicsBefore.value("/Series/Chapter 10").id);
    QCOMPARE(folders.value("/Vol 10"), foldersBefore.value("/Vol 10"));
}

void LibraryImageFoldersTest::deletesFolderComicsOnlyWhenAsked()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writePages(root.filePath("Chapter 1"), 2, 1));
    QVERIFY(writeFile(root.filePath("Book.cbz"), "book"));
    QVERIFY(writePages(root.filePath("Chapter 2"), 3, 2));
    QVERIFY(writeFile(root.filePath("Chapter 2/notes.txt"), "notes"));

    const ComicsDeletion deletion({ root.filePath("Chapter 1"), root.filePath("Book.cbz"), root.filePath("Chapter 2") });

    QCOMPARE(deletion.files, QList<int>({ 1 }));
    QCOMPARE(deletion.folders, QList<int>({ 0, 2 }));
    QCOMPARE(deletion.folderImages, 5);
    QCOMPARE(deletion.deleted(false), QList<int>({ 1 }));
    QCOMPARE(deletion.deleted(true), QList<int>({ 0, 1, 2 }));
}

void LibraryImageFoldersTest::deletesFolderComicsForGoodWhenToldTo()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writePages(root.filePath("Chapter 1"), 2, 1));
    QVERIFY(writePages(root.filePath("Chapter 2"), 3, 2));

    QStringListModel model({ "Chapter 1", "Chapter 2" });
    QModelIndexList indexes { model.index(0), model.index(1) };
    QList<QString> paths { root.filePath("Chapter 1"), root.filePath("Chapter 2") };
    ComicsRemover remover(indexes, paths, 1);
    remover.setDeleteFoldersPermanently(true);
    QSignalSpy removed(&remover, &ComicsRemover::remove);
    QSignalSpy notTrashed(&remover, &ComicsRemover::folderNotTrashed);
    QSignalSpy failed(&remover, &ComicsRemover::removeError);

    remover.process();

    QCOMPARE(removed.count(), 2);
    QCOMPARE(notTrashed.count(), 0);
    QCOMPARE(failed.count(), 0);
    QVERIFY(!QFileInfo::exists(root.filePath("Chapter 1")));
    QVERIFY(!QFileInfo::exists(root.filePath("Chapter 2")));
}

QTEST_GUILESS_MAIN(LibraryImageFoldersTest)

#include "main.moc"
