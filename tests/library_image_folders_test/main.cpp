#include "comic.h"
#include "comic_db.h"
#include "data_base_management.h"
#include "db_helper.h"
#include "folder.h"
#include "library_creator.h"
#include "library_entry.h"
#include "yacreader_global.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSet>
#include <QSettings>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <chrono>
#include <filesystem>
#include <utility>

using YACReader::LibraryEntry;
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
    void modifiedDatesFollowThePages();
    void hashesFollowThePages();
    void hashesThePagesInAnOrderOfTheirOwn();
    void leavesTheLibraryDataFolderOut();
    void folderUpdateOfAFolderThatBecameAComic();
    void sortsPagesWithoutTies();
    void keepsTheComicsOfFoldersNamedLikeMetadataFolders();
};

namespace {

struct ComicRow {
    qulonglong id;
    int numPages;
    QString hash;
    QString coverSize;
    QString title;
    qint64 added;
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

bool setLastModifiedInAnHour(const QString &path)
{
    const std::filesystem::path file(path.toStdU16String());
    std::error_code error;
    const auto lastModified = std::filesystem::last_write_time(file, error);
    std::filesystem::last_write_time(file, lastModified + std::chrono::hours(1), error);
    return !error;
}

template<typename FinishedSignal>
bool runUntil(LibraryCreator &creator, FinishedSignal finishedSignal)
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
        QSqlQuery query("SELECT c.id, c.path, ci.numPages, ci.hash, ci.originalCoverSize, ci.title, ci.added "
                        "FROM comic c INNER JOIN comic_info ci ON c.comicInfoId = ci.id",
                        db);
        while (query.next()) {
            comics.insert(query.value(1).toString(), { query.value(0).toULongLong(), query.value(2).toInt(), query.value(3).toString(), query.value(4).toString(), query.value(5).toString(), query.value(6).toLongLong() });
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
        // how a Synology NAS keeps thumbnails, in a folder in every folder
        { "/Series/Chapter 2/@eaDir/1.png", 1, 13 },
        { "/Series/Chapter 10", 6, 2 },
        { "/Mixed", 2, 4 },
        { "/Mixed/Extras", 5, 5 },
        { "/With archive", 1, 6 },
        // a user's folder, not a metadata one, whatever its first character
        { "/@Scans", 2, 7 },
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
    QTest::newRow("upper case extension") << "Chapter" << QStringList { "1.PNG" } << true;
    QTest::newRow("images and other files") << "Chapter" << QStringList { "1.png", "notes.txt", "ComicInfo.xml" } << true;
    QTest::newRow("no images") << "Chapter" << QStringList { "notes.txt" } << false;
    QTest::newRow("only dot files") << "Chapter" << QStringList { "._1.png" } << false;
    QTest::newRow("empty") << "Chapter" << QStringList { } << false;
    QTest::newRow("images and a subfolder") << "Chapter" << QStringList { "1.png", "Extras/1.png" } << false;
    QTest::newRow("images and a hidden subfolder") << "Chapter" << QStringList { "1.png", ".thumbnails/1.png" } << true;
    QTest::newRow("images and a NAS metadata subfolder") << "Chapter" << QStringList { "1.png", "@eaDir/1.png/SYNOPHOTO_THUMB_M.jpg" } << true;
    QTest::newRow("images and macOS metadata") << "Chapter" << QStringList { "1.png", "__MACOSX/._1.png" } << true;
    QTest::newRow("images and an archive") << "Chapter" << QStringList { "1.png", "Book.cbz" } << false;
    QTest::newRow("images and a PDF") << "Chapter" << QStringList { "1.png", "Book.pdf" } << false;
    QTest::newRow("folder named like an archive") << "Book.cbz" << QStringList { "1.png" } << true;
    QTest::newRow("thumbnails folder") << ".thumbnails" << QStringList { "1.png" } << false;
    QTest::newRow("NAS metadata folder") << "@eaDir" << QStringList { "1.png" } << false;
    // Synology keeps the thumbnails of each image in a folder named after it
    QTest::newRow("folder in a NAS metadata folder") << "@eaDir/1.png" << QStringList { "SYNOPHOTO_THUMB_M.jpg" } << false;
    // only exact names are metadata, a user's own folders can start with @ or a dot
    QTest::newRow("folder starting with @") << "@Comics" << QStringList { "1.png" } << true;
    QTest::newRow("images and a subfolder starting with @") << "Chapter" << QStringList { "1.png", "@Extras/1.png" } << false;
    QTest::newRow("images and a dot subfolder") << "Chapter" << QStringList { "1.png", ".Archive/Book.cbz" } << false;
    // a fixed list, so the image plugins of the app do not decide; Qt reads PPM without a plugin
    QTest::newRow("WebP images") << "Chapter" << QStringList { "1.webp" } << true;
    QTest::newRow("only images outside the list") << "Chapter" << QStringList { "1.ppm", "2.xpm" } << false;
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
    // dot files are hidden on Linux and macOS, so there the rule never sees them
    const QStringList listed = QDir(folder).entryList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    for (const auto &file : files) {
        const QString entry = file.section('/', 0, 0);
        if (entry.startsWith('.') && !listed.contains(entry)) {
            QSKIP("this system hides dot files, so the listing never shows them");
        }
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
        { "/@Scans", 2 },
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
    QVERIFY(QFile::remove(source + "/Reprint/p2.png"));

    QVERIFY(updateLibrary(source, settings));

    const auto comics = comicsByPath(source);
    const auto folders = folderIdsByPath(source);
    const QMap<QString, int> expected {
        { "/Solo/Bonus", 1 },
        { "/Series/Chapter 1", 3 },
        { "/Series/Chapter 3", 5 },
        { "/Series/Chapter 10", 6 },
        { "/Reprint", 1 },
        { "/Mixed", 2 },
        { "/@Scans", 2 },
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

void LibraryImageFoldersTest::modifiedDatesFollowThePages()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto source = root.path();
    QVERIFY(writePages(source + "/Chapter", 3, 1));
    QVERIFY(writePages(source + "/Edited", 2, 2));
    QSettings settings(root.filePath("settings.ini"), QSettings::IniFormat);
    settings.setValue(COMPARE_MODIFIED_DATE_ON_LIBRARY_UPDATES, true);
    QVERIFY(createLibrary(source, settings));
    const auto before = comicsByPath(source);

    // a new ComicInfo.xml makes the folder newer, an edited page makes only the page newer
    QVERIFY(writeFile(source + "/Chapter/ComicInfo.xml", "<ComicInfo/>"));
    QVERIFY(setLastModifiedInAnHour(source + "/Chapter"));
    QVERIFY(setLastModifiedInAnHour(source + "/Edited/2.png"));

    QVERIFY(updateLibrary(source, settings));

    // a replaced comic is dated like what changed it, an hour from now
    const auto comics = comicsByPath(source);
    QCOMPARE(comics.value("/Chapter").added, before.value("/Chapter").added);
    QVERIFY(comics.value("/Edited").added > before.value("/Edited").added + 1800);
}

void LibraryImageFoldersTest::hashesFollowThePages()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto source = root.path();
    // the same first page, names and sizes, only the bytes of the last page differ
    for (const auto &[folder, lastPage] : { std::pair { "/First", 'a' }, std::pair { "/Second", 'b' } }) {
        QVERIFY(writePages(source + folder, 1, 1));
        QVERIFY(writeFile(source + folder + "/2.png", QByteArray(100, lastPage)));
    }
    // the same pages, with a name in the two Unicode forms macOS and other systems keep it in
    for (const auto &[folder, form] : { std::pair { "/Composed", QString::NormalizationForm_C }, std::pair { "/Decomposed", QString::NormalizationForm_D } }) {
        QVERIFY(writePages(source + folder, 1, 3, QString::fromUtf8("café ").normalized(form)));
    }
    QSettings settings(root.filePath("settings.ini"), QSettings::IniFormat);

    QVERIFY(createLibrary(source, settings));

    const auto comics = comicsByPath(source);
    QCOMPARE(comics.size(), 4);
    QVERIFY(comics.value("/First").hash != comics.value("/Second").hash);
    QCOMPARE(comics.value("/Composed").hash, comics.value("/Decomposed").hash);
}

void LibraryImageFoldersTest::hashesThePagesInAnOrderOfTheirOwn()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeFile(root.filePath("Chapter/1.png"), QByteArray(10, 'a')));
    QVERIFY(writeFile(root.filePath("Chapter/2.png"), QByteArray(20, 'b')));
    QVERIFY(writeFile(root.filePath("Chapter/10.png"), QByteArray(30, 'c')));

    const auto entries = LibraryEntry::entriesIn(FolderComic::listing(root.path()));
    QCOMPARE(entries.size(), 1);
    QStringList readingOrder;
    for (const QFileInfo &page : entries.first().folderComic->pages) {
        readingOrder.append(page.fileName());
    }
    QCOMPARE(readingOrder, QStringList({ "1.png", "2.png", "10.png" }));

    // the hash takes the pages in the UTF-16 order of their names, which no locale changes
    QCryptographicHash expected(QCryptographicHash::Sha1);
    expected.addData(QByteArray(10, 'a'));
    expected.addData(QByteArray(20, 'b'));
    expected.addData("1.png/10\n10.png/30\n2.png/20\n");
    QCOMPARE(entries.first().pseudoHash(), QString(expected.result().toHex()) + "60");
}

void LibraryImageFoldersTest::leavesTheLibraryDataFolderOut()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writePages(root.filePath("Chapter"), 1, 1));
    QVERIFY(writePages(LibraryPaths::libraryCoversFolderPath(root.path()), 2, 2));
    QVERIFY(writeFile(QDir(LibraryPaths::libraryDataPath(root.path())).filePath("Backup.cbz"), "data"));

    QStringList names;
    for (const LibraryEntry &entry : LibraryEntry::entriesIn(FolderComic::listing(root.path()))) {
        names.append(entry.info.fileName());
    }

    QCOMPARE(names, QStringList { "Chapter" });
}

void LibraryImageFoldersTest::folderUpdateOfAFolderThatBecameAComic()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto source = root.path();
    QVERIFY(writePages(source + "/Arc/Part 1", 2, 1));
    QVERIFY(writePages(source + "/Arc/Part 2", 3, 2));
    QSettings settings(root.filePath("settings.ini"), QSettings::IniFormat);
    QVERIFY(createLibrary(source, settings));
    const auto arcId = folderIdsByPath(source).value("/Arc");
    QVERIFY(arcId > 1);

    QVERIFY(QDir(source + "/Arc/Part 1").removeRecursively());
    QVERIFY(QDir(source + "/Arc/Part 2").removeRecursively());
    QVERIFY(writePages(source + "/Arc", 4, 3));

    LibraryCreator creator(&settings);
    creator.updateFolder(source, LibraryPaths::libraryDataPath(source), source + "/Arc", arcId);
    QVERIFY(runUntil(creator, &LibraryCreator::updatedCurrentFolder));

    QCOMPARE(pageCounts(comicsByPath(source)), (QMap<QString, int> { { "/Arc", 4 } }));
    QVERIFY(folderIdsByPath(source).isEmpty());
}

void LibraryImageFoldersTest::sortsPagesWithoutTies()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    for (const auto &name : { "1.png", "01.png", "2.PNG", "10.png", "._1.png" }) {
        QVERIFY(writeFile(root.filePath(name), "page"));
    }

    QStringList names;
    for (const QFileInfo &page : FolderComic::pageFiles(root.path())) {
        names.append(page.fileName());
    }

    // "01" and "1" are equal for the collator, the strict order keeps them in a fixed order
    QCOMPARE(names, QStringList({ "01.png", "1.png", "2.PNG", "10.png" }));
}

void LibraryImageFoldersTest::keepsTheComicsOfFoldersNamedLikeMetadataFolders()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto source = root.path();
    QVERIFY(writePages(source + "/Solo", 2, 1));
    QSettings settings(root.filePath("settings.ini"), QSettings::IniFormat);
    QVERIFY(createLibrary(source, settings));

    // what an earlier scan imported from the user's own @ and dot folders, the archives only need
    // their size here because an update does not open an archive whose size did not change
    QStringList archives { "/@Comics/Book.cbz" };
    QVERIFY(writeFile(source + "/@Comics/Book.cbz", QByteArray(4096, 'x')));
    QVERIFY(writeFile(source + "/.Archive/Old.cbz", QByteArray(4096, 'y')));
    // dot folders are hidden on Linux and macOS, and so never part of the library there
    if (QDir(source).entryList(QDir::Dirs | QDir::NoDotAndDotDot).contains(".Archive")) {
        archives.append("/.Archive/Old.cbz");
    }
    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(LibraryPaths::libraryDataPath(source));
        connectionName = db.connectionName();
        for (const QString &path : std::as_const(archives)) {
            Folder folder(QFileInfo(path).path().mid(1), QFileInfo(path).path());
            folder.setFather(1);
            ComicDB comic;
            comic.name = QFileInfo(path).fileName();
            comic.path = path;
            comic.parentId = DBHelper::insert(&folder, db);
            // a hash of its own for each, hashes are unique in comic_info
            comic.info.hash = QString::number(archives.indexOf(path)).repeated(40) + "4096";
            comic.info.numPages = 10;
            DBHelper::insert(&comic, db, false);
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    const auto before = comicsByPath(source);
    QCOMPARE(before.size(), archives.size() + 1);

    QVERIFY(writePages(source + "/@Comics/Chapter", 2, 2));
    QVERIFY(updateLibrary(source, settings));

    const auto comics = comicsByPath(source);
    for (const QString &path : std::as_const(archives)) {
        QVERIFY2(comics.contains(path), qPrintable(path));
        QCOMPARE(comics.value(path).id, before.value(path).id);
    }
    QCOMPARE(comics.value("/@Comics/Chapter").numPages, 2);
}

QTEST_GUILESS_MAIN(LibraryImageFoldersTest)

#include "main.moc"
