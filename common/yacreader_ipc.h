#ifndef YACREADER_IPC_H
#define YACREADER_IPC_H

#include "comic_db.h"
#include "yacreader_global.h"

#include <QByteArray>
#include <QDataStream>
#include <QList>
#include <QSet>

#include <optional>

// The messages between YACReader and YACReaderLibrary on the local socket YACREADERLIBRARY_GUID.
// A message is a quint32 with the size of the rest, the message type as a quint8 and its payload. The answer
// to RequestComicInfo is framed the same way, without a type. Everything is written with QDataStream::Qt_4_8.
namespace YACReader {

enum YACReaderIPCMessages {
    RequestComicInfo = 0,
    SendComicInfo,
    // fork-only messages, numbered far from upstream's so that a future upstream message is never read as one of these
    SendComicRating = 200,
    SendComicFavorite = 201,
};

// The one byte the library answers SendComicRating and SendComicFavorite with. Types it does not know get ChangeNotSaved.
constexpr char ChangeNotSaved = 0;
constexpr char ChangeSaved = 1;

constexpr int MaxComicRating = 5;

// The payload of SendComicRating
struct ComicRatingUpdate {
    quint64 libraryId = 0;
    qulonglong comicId = 0;
    int rating = 0; // 0 clears it
};

// The payload of SendComicFavorite
struct ComicFavoriteUpdate {
    quint64 libraryId = 0;
    qulonglong comicId = 0;
    bool favorite = false;
};

// Libraries that accept SendComicRating and SendComicFavorite write it after the siblings in the answer to
// RequestComicInfo, followed by the favorite ids. Upstream libraries end the answer after the siblings.
constexpr quint32 RatingAndFavoritesMagic = 0x59524631; // "YRF1"

struct ComicInfoAnswer {
    ComicDB comic;
    QList<ComicDB> siblings;
    bool acceptsRatingAndFavorites = false;
    // the favorites of the whole library, so that the next and previous comics need no request
    QSet<qulonglong> favoriteComicIds;
};

QDataStream &operator<<(QDataStream &stream, const ComicRatingUpdate &update);
QDataStream &operator>>(QDataStream &stream, ComicRatingUpdate &update);
QDataStream &operator<<(QDataStream &stream, const ComicFavoriteUpdate &update);
QDataStream &operator>>(QDataStream &stream, ComicFavoriteUpdate &update);
QDataStream &operator<<(QDataStream &stream, const ComicInfoAnswer &answer);
QDataStream &operator>>(QDataStream &stream, ComicInfoAnswer &answer);

// The size of the fields, then the fields
template<typename... Fields>
QByteArray framed(const Fields &...fields)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_8);
    out << quint32(0);
    (out << ... << fields);
    out.device()->seek(0);
    out << quint32(block.size() - sizeof(quint32));
    return block;
}

// The fields of a framed block, once all of it has arrived
std::optional<QByteArray> framedFields(const QByteArray &received);

QByteArray comicInfoRequest(quint64 libraryId, const OpenComicSource &source, const ComicDB &comic);

}

#endif // YACREADER_IPC_H
