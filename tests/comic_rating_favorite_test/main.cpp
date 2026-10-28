#include "comic_db.h"
#include "data_base_management.h"
#include "db_helper.h"
#include "yacreader_global.h"
#include "yacreader_ipc.h"
#include "yacreader_libraries.h"
#include "yacreader_local_server.h"

#include <QDir>
#include <QFile>
#include <QLocalSocket>
#include <QMap>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <functional>

using namespace YACReader;

// Covers what the library does when the reader sets the rating or the favorite state of a comic, and the
// messages between them.
class ComicRatingFavoriteTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void setComicRatingOnlyChangesTheRatingOfThatComic();
    void setComicFavoriteAddsAndRemovesOnlyThatComic();
    void favoritesAddedAgainGoLastWithoutSharingAPlace();
    void insertComicsInFavoritesAlsoGoesAfterTheLastFavorite();
    void getFavoriteComicIdsReturnsOnlyTheFavorites();
    void withLibraryDatabaseRejectsUnknownLibraries();

    void ratingMessageWireFormat();
    void favoriteMessageWireFormat();
    void replyBytes();
    void comicInfoRequestKeepsTheUpstreamFormat();
    void comicInfoAnswerOfAnUpstreamLibraryAcceptsNoChanges();
    void comicInfoAnswerWithAnUnknownFieldAcceptsNoChanges();
    void comicInfoAnswerCarriesTheFavoritesAfterTheMagic();

    void theLibrarySavesARating();
    void theLibraryRefusesRatingsOutOfRange();
    void theLibraryRefusesUnknownLibrariesAndComics();
    void theLibraryRefusesTruncatedMessages();
    void theLibraryAnswersMessageTypesItDoesNotKnow();
    void theLibrarySavesAFavorite();
    void theAnswerToRequestComicInfoCarriesTheFavorites();
};

namespace {
QString librariesSettingsPath()
{
    return YACReader::getSettingsPath() + "/" + QCoreApplication::applicationName() + ".ini";
}

QString serverName()
{
    return QString("comic_rating_favorite_test-%1").arg(QCoreApplication::applicationPid());
}

// The comic ids and the comic_info ids are crossed, so mixing them up updates the other comic. Comic 30 is in
// another default reading list, with a higher ordering than any favorite.
bool insertFixture(QSqlDatabase &db)
{
    const QStringList statements {
        "INSERT INTO comic_info (id, hash, title, rating, currentPage) VALUES (10, 'hash-b', 'B', 2, 3)",
        "INSERT INTO comic_info (id, hash, title, rating, currentPage) VALUES (21, 'hash-a', 'A', 5, 7)",
        "INSERT INTO comic_info (id, hash, title, rating, currentPage) VALUES (30, 'hash-c', 'C', 1, 1)",
        "INSERT INTO comic (id, parentId, comicInfoId, fileName, path) VALUES (10, 1, 21, 'a.cbz', '/a.cbz')",
        "INSERT INTO comic (id, parentId, comicInfoId, fileName, path) VALUES (21, 1, 10, 'b.cbz', '/b.cbz')",
        "INSERT INTO comic (id, parentId, comicInfoId, fileName, path) VALUES (30, 1, 30, 'c.cbz', '/c.cbz')",
        "INSERT INTO comic_default_reading_list (default_reading_list_id, comic_id, ordering) VALUES (1, 21, 0)",
        "INSERT INTO default_reading_list (id, name) VALUES (2, 'Other')",
        "INSERT INTO comic_default_reading_list (default_reading_list_id, comic_id, ordering) VALUES (2, 30, 9)",
    };
    QSqlQuery query(db);
    for (const auto &statement : statements) {
        if (!query.exec(statement)) {
            qWarning() << statement << query.lastError().text();
            return false;
        }
    }
    return true;
}

// A library with the fixture, registered like YACReaderLibrary does, so that DBHelper finds it by its id
class TestLibrary
{
public:
    TestLibrary()
    {
        const QString path = directory.path();
        QDir().mkpath(LibraryPaths::libraryDataPath(path));
        {
            auto db = DataBaseManagement::createDatabase(LibraryPaths::libraryDatabasePath(path));
            connectionName = db.connectionName();
            valid = directory.isValid() && db.isValid() && insertFixture(db);
        }
        // DBHelper::withLibraryDatabase opens the same file under the name createDatabase uses in this thread
        QSqlDatabase::removeDatabase(connectionName);
        connectionName = "comic_rating_favorite_test";
        auto db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(LibraryPaths::libraryDatabasePath(path));
        valid = db.open() && QSqlQuery("PRAGMA foreign_keys = ON", db).isActive() && valid;

        YACReaderLibraries libraries;
        libraries.load();
        libraries.addLibrary("Test", path);
        valid = libraries.save() && valid;
        id = libraries.getId("Test");
    }

    ~TestLibrary()
    {
        YACReaderLibraries libraries;
        libraries.load();
        libraries.remove("Test");
        libraries.save();
        QSqlDatabase::removeDatabase(connectionName);
    }

    QSqlDatabase db() const { return QSqlDatabase::database(connectionName); }

    QTemporaryDir directory;
    QString connectionName;
    bool valid = false;
    quint64 id = 0;
};

QVariant comicInfoValue(const QSqlDatabase &db, int comicInfoId, const QString &column)
{
    QSqlQuery query(db);
    query.prepare(QString("SELECT %1 FROM comic_info WHERE id = :id").arg(column));
    query.bindValue(":id", comicInfoId);
    query.exec();
    query.next();
    return query.value(0);
}

QMap<qulonglong, int> listOrdering(const QSqlDatabase &db, int defaultReadingListId)
{
    QMap<qulonglong, int> ordering;
    QSqlQuery query(db);
    query.prepare("SELECT comic_id, ordering FROM comic_default_reading_list WHERE default_reading_list_id = :id");
    query.bindValue(":id", defaultReadingListId);
    query.exec();
    while (query.next())
        ordering.insert(query.value(0).toULongLong(), query.value(1).toInt());
    return ordering;
}

QMap<qulonglong, int> favoritesOrdering(const QSqlDatabase &db)
{
    return listOrdering(db, 1);
}

ComicDB comicWithId(qulonglong id)
{
    ComicDB comic;
    comic.id = id;
    comic.info.rating = 3;
    return comic;
}

QByteArray serialized(const auto &...fields)
{
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_8);
    (out << ... << fields);
    return bytes;
}

// Sends the message to the library and returns what it answered. The library accepts connections in this
// thread's event loop, so the wait keeps that running.
QByteArray exchange(const QByteArray &message, const std::function<bool(const QByteArray &)> &isComplete)
{
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(1000))
        return { };
    socket.write(message);
    QByteArray received;
    const auto answered = [&] {
        received += socket.readAll();
        return isComplete(received);
    };
    if (!QTest::qWaitFor(answered, 3000))
        qWarning() << "The library did not answer in time";
    return received;
}

// the one byte the library answers a change with, or -1 when it does not answer
int replyTo(const QByteArray &message)
{
    const auto received = exchange(message, [](const QByteArray &received) { return !received.isEmpty(); });
    return received.isEmpty() ? -1 : received.at(0);
}
}

void ComicRatingFavoriteTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QFile::remove(librariesSettingsPath());
    qRegisterMetaType<ComicDB>("ComicDB");
}

void ComicRatingFavoriteTest::cleanupTestCase()
{
    QFile::remove(librariesSettingsPath());
}

void ComicRatingFavoriteTest::setComicRatingOnlyChangesTheRatingOfThatComic()
{
    TestLibrary library;
    QVERIFY(library.valid);
    auto db = library.db();

    QVERIFY(DBHelper::setComicRating(10, 4, db));

    QCOMPARE(comicInfoValue(db, 21, "rating").toInt(), 4);
    QCOMPARE(comicInfoValue(db, 21, "currentPage").toInt(), 7);
    QCOMPARE(comicInfoValue(db, 21, "title").toString(), QString("A"));
    QCOMPARE(comicInfoValue(db, 10, "rating").toInt(), 2);

    QVERIFY(DBHelper::setComicRating(10, 0, db));
    QCOMPARE(comicInfoValue(db, 21, "rating").toInt(), 0);
    QCOMPARE(comicInfoValue(db, 10, "rating").toInt(), 2);

    // a comic that is no longer in the library
    QVERIFY(!DBHelper::setComicRating(99, 3, db));
}

void ComicRatingFavoriteTest::setComicFavoriteAddsAndRemovesOnlyThatComic()
{
    TestLibrary library;
    QVERIFY(library.valid);
    auto db = library.db();

    // the ordering of comic 30 in the other list does not count
    QVERIFY(DBHelper::setComicFavorite(10, true, db));
    QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 21, 0 }, { 10, 1 } }));

    // a comic that is already a favorite keeps its place in the list
    QVERIFY(DBHelper::setComicFavorite(10, true, db));
    QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 21, 0 }, { 10, 1 } }));

    QVERIFY(DBHelper::setComicFavorite(21, false, db));
    QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 10, 1 } }));

    // removing it again is fine, because the reader repeats a removal when the heart is toggled twice
    QVERIFY(DBHelper::setComicFavorite(21, false, db));
    QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 10, 1 } }));

    // a comic that is no longer in the library
    QVERIFY(!DBHelper::setComicFavorite(99, true, db));
    QVERIFY(!DBHelper::setComicFavorite(99, false, db));
    QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 10, 1 } }));

    QCOMPARE(listOrdering(db, 2), (QMap<qulonglong, int> { { 30, 9 } }));
}

void ComicRatingFavoriteTest::favoritesAddedAgainGoLastWithoutSharingAPlace()
{
    TestLibrary library;
    QVERIFY(library.valid);
    auto db = library.db();

    QVERIFY(DBHelper::setComicFavorite(10, true, db));
    QVERIFY(DBHelper::setComicFavorite(30, true, db));
    QVERIFY(DBHelper::setComicFavorite(21, false, db));
    QVERIFY(DBHelper::setComicFavorite(21, true, db));

    // counting the favorites would give 21 the place of 30
    QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 10, 1 }, { 30, 2 }, { 21, 3 } }));
}

void ComicRatingFavoriteTest::insertComicsInFavoritesAlsoGoesAfterTheLastFavorite()
{
    TestLibrary library;
    QVERIFY(library.valid);
    auto db = library.db();

    QVERIFY(DBHelper::setComicFavorite(10, true, db));
    QVERIFY(DBHelper::setComicFavorite(21, false, db));

    // counting the favorites would give 30 the place of 10; 10 is already a favorite and keeps its place
    DBHelper::insertComicsInFavorites({ comicWithId(30), comicWithId(10) }, db);
    QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 10, 1 }, { 30, 2 } }));
}

void ComicRatingFavoriteTest::getFavoriteComicIdsReturnsOnlyTheFavorites()
{
    TestLibrary library;
    QVERIFY(library.valid);
    auto db = library.db();
    QVERIFY(DBHelper::setComicFavorite(10, true, db));

    // 30 is only in the other list
    QCOMPARE(DBHelper::getFavoriteComicIds(db), (QSet<qulonglong> { 21, 10 }));
}

void ComicRatingFavoriteTest::withLibraryDatabaseRejectsUnknownLibraries()
{
    TestLibrary library;
    QVERIFY(library.valid);

    bool ran = false;
    QVERIFY(DBHelper::withLibraryDatabase(library.id, [&ran](QSqlDatabase &db) { ran = db.isOpen(); }));
    QVERIFY(ran);

    // the empty path of an unknown library would find the library in the working directory
    const QString workingDirectory = QDir::currentPath();
    QVERIFY(QDir::setCurrent(library.directory.path()));
    ran = false;
    const bool opened = DBHelper::withLibraryDatabase(library.id + 1, [&ran](QSqlDatabase &) { ran = true; });
    QDir::setCurrent(workingDirectory);
    QVERIFY(!opened);
    QVERIFY(!ran);
}

void ComicRatingFavoriteTest::ratingMessageWireFormat()
{
    // size of the rest, type 200, library id, comic id, rating as qint32
    QCOMPARE(framed(quint8(SendComicRating), ComicRatingUpdate { 3, 41, 4 }).toHex(),
             QByteArray("00000015"
                        "c8"
                        "0000000000000003"
                        "0000000000000029"
                        "00000004"));

    QDataStream server(serialized(quint64(7), qulonglong(52), qint32(1)));
    server.setVersion(QDataStream::Qt_4_8);
    ComicRatingUpdate update;
    server >> update;
    QCOMPARE(server.status(), QDataStream::Ok);
    QCOMPARE(update.libraryId, 7ULL);
    QCOMPARE(update.comicId, 52ULL);
    QCOMPARE(update.rating, 1);
}

void ComicRatingFavoriteTest::favoriteMessageWireFormat()
{
    // size of the rest, type 201, library id, comic id, favorite as one byte
    QCOMPARE(framed(quint8(SendComicFavorite), ComicFavoriteUpdate { 3, 41, true }).toHex(),
             QByteArray("00000012"
                        "c9"
                        "0000000000000003"
                        "0000000000000029"
                        "01"));

    QDataStream server(serialized(quint64(7), qulonglong(52), false));
    server.setVersion(QDataStream::Qt_4_8);
    ComicFavoriteUpdate update { 0, 0, true };
    server >> update;
    QCOMPARE(server.status(), QDataStream::Ok);
    QCOMPARE(update.libraryId, 7ULL);
    QCOMPARE(update.comicId, 52ULL);
    QCOMPARE(update.favorite, false);
}

void ComicRatingFavoriteTest::replyBytes()
{
    QCOMPARE(int(ChangeSaved), 1);
    QCOMPARE(int(ChangeNotSaved), 0);
}

void ComicRatingFavoriteTest::comicInfoRequestKeepsTheUpstreamFormat()
{
    const OpenComicSource source { OpenComicSource::ReadingList, 8 };
    const auto comic = comicWithId(5);

    // what upstream's YACReaderLocalClient::requestComicInfo writes
    QByteArray upstream;
    QDataStream out(&upstream, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_8);
    out << (quint32)0;
    out << (quint8)RequestComicInfo;
    out << quint64(3);
    out << source;
    out << comic;
    out.device()->seek(0);
    out << (quint32)(upstream.size() - sizeof(quint32));

    QCOMPARE(comicInfoRequest(3, source, comic), upstream);
    QCOMPARE(framedFields(upstream), std::optional<QByteArray>(upstream.mid(4)));
    QVERIFY(!framedFields(upstream.chopped(1)).has_value());
}

void ComicRatingFavoriteTest::comicInfoAnswerOfAnUpstreamLibraryAcceptsNoChanges()
{
    QDataStream reader(serialized(comicWithId(5), QList<ComicDB> { comicWithId(5), comicWithId(6) }));
    reader.setVersion(QDataStream::Qt_4_8);
    ComicInfoAnswer answer { ComicDB(), { }, true, { 5 } };
    reader >> answer;

    QCOMPARE(answer.comic.id, 5ULL);
    QCOMPARE(answer.siblings.size(), 2);
    QVERIFY(!answer.acceptsRatingAndFavorites);
    QVERIFY(answer.favoriteComicIds.isEmpty());
}

void ComicRatingFavoriteTest::comicInfoAnswerWithAnUnknownFieldAcceptsNoChanges()
{
    // a field that a future upstream library could add after the siblings, followed by what looks like favorites
    QDataStream reader(serialized(comicWithId(5), QList<ComicDB> { comicWithId(5) }, quint32(0x12345678), QSet<qulonglong> { 5 }));
    reader.setVersion(QDataStream::Qt_4_8);
    ComicInfoAnswer answer;
    reader >> answer;

    QCOMPARE(answer.comic.id, 5ULL);
    QVERIFY(!answer.acceptsRatingAndFavorites);
    QVERIFY(answer.favoriteComicIds.isEmpty());
}

void ComicRatingFavoriteTest::comicInfoAnswerCarriesTheFavoritesAfterTheMagic()
{
    const ComicInfoAnswer sent { comicWithId(5), { comicWithId(5), comicWithId(6) }, true, { 6, 9 } };
    const auto upstreamPart = serialized(sent.comic, sent.siblings);
    const auto written = serialized(sent);

    QVERIFY(written.startsWith(upstreamPart));
    QCOMPARE(written.mid(upstreamPart.size(), 4).toHex(), QByteArray("59524631"));
    QCOMPARE(written.mid(upstreamPart.size() + 4), serialized(sent.favoriteComicIds));

    QDataStream reader(written);
    reader.setVersion(QDataStream::Qt_4_8);
    ComicInfoAnswer answer;
    reader >> answer;
    QCOMPARE(answer.comic.id, 5ULL);
    QCOMPARE(answer.siblings.at(1).id, 6ULL);
    QVERIFY(answer.acceptsRatingAndFavorites);
    QCOMPARE(answer.favoriteComicIds, (QSet<qulonglong> { 6, 9 }));
}

void ComicRatingFavoriteTest::theLibrarySavesARating()
{
    TestLibrary library;
    QVERIFY(library.valid);
    YACReaderLocalServer server(nullptr, serverName());
    QVERIFY(server.isListening());
    QSignalSpy comicUpdated(&server, &YACReaderLocalServer::comicUpdated);

    QCOMPARE(replyTo(framed(quint8(SendComicRating), ComicRatingUpdate { library.id, 10, 4 })), int(ChangeSaved));

    QCOMPARE(comicInfoValue(library.db(), 21, "rating").toInt(), 4);
    QTRY_COMPARE(comicUpdated.count(), 1);
    const auto comic = comicUpdated.at(0).at(1).value<ComicDB>();
    QCOMPARE(comicUpdated.at(0).at(0).toULongLong(), library.id);
    QCOMPARE(comic.id, 10ULL);
    QCOMPARE(comic.info.rating, 4);
}

void ComicRatingFavoriteTest::theLibraryRefusesRatingsOutOfRange()
{
    TestLibrary library;
    QVERIFY(library.valid);
    YACReaderLocalServer server(nullptr, serverName());

    QCOMPARE(replyTo(framed(quint8(SendComicRating), ComicRatingUpdate { library.id, 10, MaxComicRating + 1 })), int(ChangeNotSaved));
    QCOMPARE(replyTo(framed(quint8(SendComicRating), ComicRatingUpdate { library.id, 10, -1 })), int(ChangeNotSaved));
    QCOMPARE(comicInfoValue(library.db(), 21, "rating").toInt(), 5);
}

void ComicRatingFavoriteTest::theLibraryRefusesUnknownLibrariesAndComics()
{
    TestLibrary library;
    QVERIFY(library.valid);
    YACReaderLocalServer server(nullptr, serverName());

    QCOMPARE(replyTo(framed(quint8(SendComicRating), ComicRatingUpdate { library.id + 1, 10, 4 })), int(ChangeNotSaved));
    QCOMPARE(replyTo(framed(quint8(SendComicFavorite), ComicFavoriteUpdate { library.id + 1, 10, true })), int(ChangeNotSaved));
    QCOMPARE(replyTo(framed(quint8(SendComicRating), ComicRatingUpdate { library.id, 99, 4 })), int(ChangeNotSaved));
    QCOMPARE(replyTo(framed(quint8(SendComicFavorite), ComicFavoriteUpdate { library.id, 99, true })), int(ChangeNotSaved));
    QCOMPARE(comicInfoValue(library.db(), 21, "rating").toInt(), 5);
}

void ComicRatingFavoriteTest::theLibraryRefusesTruncatedMessages()
{
    TestLibrary library;
    QVERIFY(library.valid);
    YACReaderLocalServer server(nullptr, serverName());

    QCOMPARE(replyTo(framed(quint8(SendComicRating), quint64(library.id), qulonglong(10))), int(ChangeNotSaved));
    QCOMPARE(replyTo(framed(quint8(SendComicFavorite), quint64(library.id))), int(ChangeNotSaved));
    QCOMPARE(comicInfoValue(library.db(), 21, "rating").toInt(), 5);
}

void ComicRatingFavoriteTest::theLibraryAnswersMessageTypesItDoesNotKnow()
{
    TestLibrary library;
    QVERIFY(library.valid);
    YACReaderLocalServer server(nullptr, serverName());

    // a message of a newer reader gets an answer at once instead of none
    QCOMPARE(replyTo(framed(quint8(SendComicFavorite + 1), ComicRatingUpdate { library.id, 10, 4 })), int(ChangeNotSaved));
    QCOMPARE(comicInfoValue(library.db(), 21, "rating").toInt(), 5);
}

void ComicRatingFavoriteTest::theLibrarySavesAFavorite()
{
    TestLibrary library;
    QVERIFY(library.valid);
    YACReaderLocalServer server(nullptr, serverName());
    QSignalSpy favoriteChanged(&server, &YACReaderLocalServer::comicFavoriteChanged);

    QCOMPARE(replyTo(framed(quint8(SendComicFavorite), ComicFavoriteUpdate { library.id, 10, true })), int(ChangeSaved));
    QCOMPARE(favoritesOrdering(library.db()), (QMap<qulonglong, int> { { 21, 0 }, { 10, 1 } }));
    QCOMPARE(replyTo(framed(quint8(SendComicFavorite), ComicFavoriteUpdate { library.id, 21, false })), int(ChangeSaved));
    QCOMPARE(favoritesOrdering(library.db()), (QMap<qulonglong, int> { { 10, 1 } }));

    QTRY_COMPARE(favoriteChanged.count(), 2);
    QCOMPARE(favoriteChanged.at(0), (QList<QVariant> { library.id, 10ULL }));
    QCOMPARE(favoriteChanged.at(1), (QList<QVariant> { library.id, 21ULL }));
}

void ComicRatingFavoriteTest::theAnswerToRequestComicInfoCarriesTheFavorites()
{
    TestLibrary library;
    QVERIFY(library.valid);
    YACReaderLocalServer server(nullptr, serverName());

    const auto received = exchange(comicInfoRequest(library.id, { OpenComicSource::Folder, 1 }, comicWithId(10)),
                                   [](const QByteArray &received) { return framedFields(received).has_value(); });
    const auto fields = framedFields(received);
    QVERIFY(fields.has_value());

    QDataStream reader(*fields);
    reader.setVersion(QDataStream::Qt_4_8);
    ComicInfoAnswer answer;
    reader >> answer;
    QCOMPARE(answer.comic.id, 10ULL);
    QCOMPARE(answer.comic.info.rating, 5);
    QVERIFY(answer.acceptsRatingAndFavorites);
    QCOMPARE(answer.favoriteComicIds, (QSet<qulonglong> { 21 }));
}

QTEST_GUILESS_MAIN(ComicRatingFavoriteTest)

#include "main.moc"
