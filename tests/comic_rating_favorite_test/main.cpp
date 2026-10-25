#include "comic_db.h"
#include "data_base_management.h"
#include "db_helper.h"
#include "yacreader_global.h"

#include <QMap>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

// Covers what the library does when the reader sets the rating or the favorite state of a comic.
class ComicRatingFavoriteTest : public QObject
{
    Q_OBJECT

private slots:
    void selectingTheCurrentRatingClearsIt();
    void setComicRatingOnlyChangesTheRatingOfThatComic();
    void setComicFavoriteAddsAndRemovesOnlyThatComic();
    void favoritesAddedAgainGoLastWithoutSharingAPlace();
    void getFavoriteComicIdsReturnsOnlyTheFavorites();
    void ratingUpdateWireFormat();
    void favoriteUpdateWireFormat();
    void comicInfoAnswerOfAnOlderLibraryHasNoFavorites();
    void comicInfoAnswerCarriesTheFavorites();
};

namespace {
// The comic ids and the comic_info ids are crossed, so mixing them up updates the other comic.
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
        "INSERT INTO label (id, name, color, ordering) VALUES (1, 'Label', 'red', 1)",
        "INSERT INTO comic_label (comic_id, label_id, ordering) VALUES (30, 1, 0)",
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

QVariant comicInfoValue(QSqlDatabase &db, int comicInfoId, const QString &column)
{
    QSqlQuery query(db);
    query.prepare(QString("SELECT %1 FROM comic_info WHERE id = :id").arg(column));
    query.bindValue(":id", comicInfoId);
    query.exec();
    query.next();
    return query.value(0);
}

QMap<qulonglong, int> favoritesOrdering(QSqlDatabase &db)
{
    QMap<qulonglong, int> ordering;
    QSqlQuery query("SELECT comic_id, ordering FROM comic_default_reading_list WHERE default_reading_list_id = 1", db);
    while (query.next())
        ordering.insert(query.value(0).toULongLong(), query.value(1).toInt());
    return ordering;
}

ComicDB comicWithId(qulonglong id)
{
    ComicDB comic;
    comic.id = id;
    comic.info.rating = 3;
    return comic;
}
}

void ComicRatingFavoriteTest::selectingTheCurrentRatingClearsIt()
{
    QCOMPARE(YACReader::ratingAfterSelecting(2, 4), 4);
    QCOMPARE(YACReader::ratingAfterSelecting(0, 3), 3);
    QCOMPARE(YACReader::ratingAfterSelecting(4, 4), 0);
}

void ComicRatingFavoriteTest::setComicRatingOnlyChangesTheRatingOfThatComic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString connectionName;
    {
        auto db = DataBaseManagement::createDatabase(directory.filePath("library.ydb"));
        connectionName = db.connectionName();
        QVERIFY(insertFixture(db));

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
    QSqlDatabase::removeDatabase(connectionName);
}

void ComicRatingFavoriteTest::setComicFavoriteAddsAndRemovesOnlyThatComic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString connectionName;
    {
        auto db = DataBaseManagement::createDatabase(directory.filePath("library.ydb"));
        connectionName = db.connectionName();
        QVERIFY(insertFixture(db));

        QVERIFY(DBHelper::setComicFavorite(10, true, db));
        QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 21, 0 }, { 10, 1 } }));

        // a comic that is already a favorite keeps its place in the list
        QVERIFY(DBHelper::setComicFavorite(10, true, db));
        QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 21, 0 }, { 10, 1 } }));

        QVERIFY(DBHelper::setComicFavorite(21, false, db));
        QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 10, 1 } }));

        // a comic that is no longer in the library
        QVERIFY(!DBHelper::setComicFavorite(99, true, db));
        QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 10, 1 } }));
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ComicRatingFavoriteTest::favoritesAddedAgainGoLastWithoutSharingAPlace()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString connectionName;
    {
        auto db = DataBaseManagement::createDatabase(directory.filePath("library.ydb"));
        connectionName = db.connectionName();
        QVERIFY(insertFixture(db));

        QVERIFY(DBHelper::setComicFavorite(10, true, db));
        QVERIFY(DBHelper::setComicFavorite(30, true, db));
        QVERIFY(DBHelper::setComicFavorite(21, false, db));
        QVERIFY(DBHelper::setComicFavorite(21, true, db));

        // counting the favorites would give 21 the place of 30
        QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 10, 1 }, { 30, 2 }, { 21, 3 } }));
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ComicRatingFavoriteTest::getFavoriteComicIdsReturnsOnlyTheFavorites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString connectionName;
    {
        auto db = DataBaseManagement::createDatabase(directory.filePath("library.ydb"));
        connectionName = db.connectionName();
        QVERIFY(insertFixture(db));
        QVERIFY(DBHelper::setComicFavorite(10, true, db));

        // 30 is only in a label
        QCOMPARE(DBHelper::getFavoriteComicIds(db), (QSet<qulonglong> { 21, 10 }));
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ComicRatingFavoriteTest::ratingUpdateWireFormat()
{
    QByteArray written;
    QDataStream out(&written, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_8);
    out << YACReader::ComicRatingUpdate { 3, 41, 4 };

    QDataStream fields(written);
    quint64 libraryId;
    qulonglong comicId;
    qint32 rating;
    fields >> libraryId >> comicId >> rating;
    QCOMPARE(libraryId, 3ULL);
    QCOMPARE(comicId, 41ULL);
    QCOMPARE(rating, 4);
    QVERIFY(fields.atEnd());

    QByteArray sent;
    QDataStream client(&sent, QIODevice::WriteOnly);
    client.setVersion(QDataStream::Qt_4_8);
    client << quint64(7) << qulonglong(52) << qint32(1);
    QDataStream server(sent);
    YACReader::ComicRatingUpdate update;
    server >> update;
    QCOMPARE(server.status(), QDataStream::Ok);
    QCOMPARE(update.libraryId, 7ULL);
    QCOMPARE(update.comicId, 52ULL);
    QCOMPARE(update.rating, 1);
}

void ComicRatingFavoriteTest::favoriteUpdateWireFormat()
{
    QByteArray written;
    QDataStream out(&written, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_8);
    out << YACReader::ComicFavoriteUpdate { 3, 41, true };

    QDataStream fields(written);
    quint64 libraryId;
    qulonglong comicId;
    bool favorite;
    fields >> libraryId >> comicId >> favorite;
    QCOMPARE(libraryId, 3ULL);
    QCOMPARE(comicId, 41ULL);
    QCOMPARE(favorite, true);
    QVERIFY(fields.atEnd());

    QByteArray sent;
    QDataStream client(&sent, QIODevice::WriteOnly);
    client.setVersion(QDataStream::Qt_4_8);
    client << quint64(7) << qulonglong(52) << false;
    QDataStream server(sent);
    YACReader::ComicFavoriteUpdate update { 0, 0, true };
    server >> update;
    QCOMPARE(server.status(), QDataStream::Ok);
    QCOMPARE(update.libraryId, 7ULL);
    QCOMPARE(update.comicId, 52ULL);
    QCOMPARE(update.favorite, false);
}

void ComicRatingFavoriteTest::comicInfoAnswerOfAnOlderLibraryHasNoFavorites()
{
    QByteArray answer;
    QDataStream library(&answer, QIODevice::WriteOnly);
    library.setVersion(QDataStream::Qt_4_8);
    library << comicWithId(5) << QList<ComicDB> { comicWithId(5), comicWithId(6) };

    QDataStream reader(answer);
    reader.setVersion(QDataStream::Qt_4_8);
    ComicInfoAnswer read;
    read.favoriteComicIds = QSet<qulonglong> { 5 };
    reader >> read;

    QCOMPARE(read.comic.id, 5ULL);
    QCOMPARE(read.siblings.size(), 2);
    QVERIFY(!read.favoriteComicIds.has_value());
}

void ComicRatingFavoriteTest::comicInfoAnswerCarriesTheFavorites()
{
    QByteArray answer;
    QDataStream library(&answer, QIODevice::WriteOnly);
    library.setVersion(QDataStream::Qt_4_8);
    library << ComicInfoAnswer { comicWithId(5), { comicWithId(5), comicWithId(6) }, QSet<qulonglong> { 6, 9 } };

    QDataStream reader(answer);
    reader.setVersion(QDataStream::Qt_4_8);
    ComicInfoAnswer read;
    reader >> read;

    QCOMPARE(read.comic.id, 5ULL);
    QCOMPARE(read.siblings.size(), 2);
    QCOMPARE(read.siblings.at(1).id, 6ULL);
    QVERIFY(read.favoriteComicIds.has_value());
    QCOMPARE(*read.favoriteComicIds, (QSet<qulonglong> { 6, 9 }));
}

QTEST_GUILESS_MAIN(ComicRatingFavoriteTest)

#include "main.moc"
