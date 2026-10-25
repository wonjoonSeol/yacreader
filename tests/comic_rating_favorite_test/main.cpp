#include "data_base_management.h"
#include "db_helper.h"
#include "yacreader_global.h"

#include <QMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

// Covers what the library does when the reader sets the rating or the favorite state of a comic.
class ComicRatingFavoriteTest : public QObject
{
    Q_OBJECT

private slots:
    void setComicRatingOnlyChangesTheRatingOfThatComic();
    void setComicFavoriteAddsAndRemovesOnlyThatComic();
    void ratingUpdateWireFormat();
    void favoriteUpdateWireFormat();
};

namespace {
// The comic ids and the comic_info ids are crossed, so mixing them up updates the other comic.
QSqlDatabase createLibraryDatabase(const QString &path)
{
    auto db = DataBaseManagement::createDatabase(path);

    QSqlQuery query(db);
    query.exec("INSERT INTO comic_info (id, hash, title, rating, currentPage) VALUES (10, 'hash-b', 'B', 2, 3)");
    query.exec("INSERT INTO comic_info (id, hash, title, rating, currentPage) VALUES (21, 'hash-a', 'A', 5, 7)");
    query.exec("INSERT INTO comic (id, parentId, comicInfoId, fileName, path) VALUES (10, 1, 21, 'a.cbz', '/a.cbz')");
    query.exec("INSERT INTO comic (id, parentId, comicInfoId, fileName, path) VALUES (21, 1, 10, 'b.cbz', '/b.cbz')");
    query.exec("INSERT INTO comic_default_reading_list (default_reading_list_id, comic_id, ordering) VALUES (1, 21, 0)");
    return db;
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
}

void ComicRatingFavoriteTest::setComicRatingOnlyChangesTheRatingOfThatComic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString connectionName;
    {
        auto db = createLibraryDatabase(directory.filePath("library.ydb"));
        connectionName = db.connectionName();

        QVERIFY(DBHelper::setComicRating(10, 4, db));

        QCOMPARE(comicInfoValue(db, 21, "rating").toInt(), 4);
        QCOMPARE(comicInfoValue(db, 21, "currentPage").toInt(), 7);
        QCOMPARE(comicInfoValue(db, 21, "title").toString(), QString("A"));
        QCOMPARE(comicInfoValue(db, 10, "rating").toInt(), 2);

        QVERIFY(DBHelper::setComicRating(10, 0, db));
        QCOMPARE(comicInfoValue(db, 21, "rating").toInt(), 0);
        QCOMPARE(comicInfoValue(db, 10, "rating").toInt(), 2);
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ComicRatingFavoriteTest::setComicFavoriteAddsAndRemovesOnlyThatComic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString connectionName;
    {
        auto db = createLibraryDatabase(directory.filePath("library.ydb"));
        connectionName = db.connectionName();

        DBHelper::setComicFavorite(10, true, db);
        QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 21, 0 }, { 10, 1 } }));
        QVERIFY(DBHelper::isFavoriteComic(10, db));

        // a comic that is already a favorite keeps its place in the list
        DBHelper::setComicFavorite(10, true, db);
        QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 21, 0 }, { 10, 1 } }));

        DBHelper::setComicFavorite(21, false, db);
        QCOMPARE(favoritesOrdering(db), (QMap<qulonglong, int> { { 10, 1 } }));
        QVERIFY(!DBHelper::isFavoriteComic(21, db));
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ComicRatingFavoriteTest::ratingUpdateWireFormat()
{
    QByteArray written;
    QDataStream out(&written, QIODevice::WriteOnly);
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
    client << quint64(7) << qulonglong(52) << qint32(1);
    QDataStream server(sent);
    YACReader::ComicRatingUpdate update { };
    server >> update;
    QCOMPARE(update.libraryId, 7ULL);
    QCOMPARE(update.comicId, 52ULL);
    QCOMPARE(update.rating, 1);
}

void ComicRatingFavoriteTest::favoriteUpdateWireFormat()
{
    QByteArray written;
    QDataStream out(&written, QIODevice::WriteOnly);
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
    client << quint64(7) << qulonglong(52) << false;
    QDataStream server(sent);
    YACReader::ComicFavoriteUpdate update { 0, 0, true };
    server >> update;
    QCOMPARE(update.libraryId, 7ULL);
    QCOMPARE(update.comicId, 52ULL);
    QCOMPARE(update.favorite, false);
}

QTEST_GUILESS_MAIN(ComicRatingFavoriteTest)

#include "main.moc"
