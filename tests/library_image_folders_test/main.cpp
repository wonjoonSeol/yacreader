#include "comic.h"
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

// pages with the same number look the same in comics made with the same seed
bool writePages(const QString &folder, int count, int seed)
{
    if (!QDir().mkpath(folder)) {
        return false;
    }
    for (int page = 1; page <= count; ++page) {
        QImage image(pageSize(page), QImage::Format_RGB32);
        image.fill(QColor::fromHsv((seed * 40 + page * 7) % 360, 120, 220));
        if (!image.save(QDir(folder).filePath(QStringLiteral("%1.png").arg(page)))) {
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

bool runCreator(const QString &source, QSettings &settings, bool update)
{
    LibraryCreator creator(&settings);
    QSignalSpy finished(&creator, update ? &LibraryCreator::updated : &LibraryCreator::created);
    const auto target = LibraryPaths::libraryDataPath(source);
    if (update) {
        creator.updateLibrary(source, target);
    } else {
        creator.createLibrary(source, target);
    }
    creator.start();
    return creator.wait() && finished.count() == 1;
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

QStringList folderPaths(const QString &source)
{
    QStringList paths;
    QString connectionName;
    {
        auto db = DataBaseManagement::loadDatabase(LibraryPaths::libraryDataPath(source));
        connectionName = db.connectionName();
        QSqlQuery query("SELECT path FROM folder WHERE id > 1", db);
        while (query.next()) {
            paths.append(query.value(0).toString());
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    paths.sort();
    return paths;
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

// Chapter 1 and Chapter 10 share their first pages, so only their page lists tell them apart
bool buildLibrary(const QString &source)
{
    return writePages(source + "/Solo", 3, 1) && writeFile(source + "/Solo/notes.txt", "notes") && writeFile(source + "/Solo/ComicInfo.xml", "<ComicInfo><Title>Solo Title</Title></ComicInfo>") && writePages(source + "/Series/Chapter 1", 2, 2) && writePages(source + "/Series/Chapter 2", 4, 3) && writePages(source + "/Series/Chapter 10", 6, 2) && writeFile(source + "/Notes/readme.txt", "readme") && writePages(source + "/Mixed", 2, 4) && writePages(source + "/Mixed/Extras", 5, 5) && writePages(source + "/With archive", 1, 6) && writeFile(source + "/With archive/Book.cbz", QByteArray(4096, 'x')) && writePages(source + "/.hidden", 2, 7);
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
        const auto path = QDir(folder).filePath(file);
        if (file.endsWith(".png") || file.endsWith(".jpg")) {
            QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
            QImage image(pageSize(1), QImage::Format_RGB32);
            image.fill(Qt::white);
            QVERIFY(image.save(path));
        } else {
            QVERIFY(writeFile(path, "data"));
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

    QVERIFY(runCreator(source, settings, false));

    const auto comics = comicsByPath(source);
    const QMap<QString, int> expected {
        { "/Solo", 3 },
        { "/Series/Chapter 1", 2 },
        { "/Series/Chapter 2", 4 },
        { "/Series/Chapter 10", 6 },
        { "/Mixed/Extras", 5 },
    };
    QCOMPARE(pageCounts(comics), expected);
    QCOMPARE(folderPaths(source), QStringList({ "/Mixed", "/Series" }));
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
    QVERIFY(runCreator(source, settings, false));
    const auto before = comicsByPath(source);

    QVERIFY(writePages(source + "/Series/Chapter 1", 3, 2));
    QVERIFY(QDir(source + "/Series/Chapter 2").removeRecursively());
    QVERIFY(writePages(source + "/Series/Chapter 3", 5, 8));
    QVERIFY(writePages(source + "/Solo/Bonus", 1, 9));
    QVERIFY(QDir(source + "/Mixed/Extras").removeRecursively());

    QVERIFY(runCreator(source, settings, true));

    const auto comics = comicsByPath(source);
    const QMap<QString, int> expected {
        { "/Solo/Bonus", 1 },
        { "/Series/Chapter 1", 3 },
        { "/Series/Chapter 3", 5 },
        { "/Series/Chapter 10", 6 },
        { "/Mixed", 2 },
    };
    QCOMPARE(pageCounts(comics), expected);
    QCOMPARE(folderPaths(source), QStringList({ "/Series", "/Solo" }));
    verifyCoversAndHashes(source, comics);
    QVERIFY(comics.value("/Series/Chapter 1").hash != before.value("/Series/Chapter 1").hash);
    QCOMPARE(comics.value("/Series/Chapter 10").hash, before.value("/Series/Chapter 10").hash);
}

QTEST_GUILESS_MAIN(LibraryImageFoldersTest)

#include "main.moc"
