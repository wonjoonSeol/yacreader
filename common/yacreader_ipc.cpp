#include "yacreader_ipc.h"

QDataStream &YACReader::operator<<(QDataStream &stream, const ComicRatingUpdate &update)
{
    return stream << update.libraryId << update.comicId << update.rating;
}

QDataStream &YACReader::operator>>(QDataStream &stream, ComicRatingUpdate &update)
{
    return stream >> update.libraryId >> update.comicId >> update.rating;
}

QDataStream &YACReader::operator<<(QDataStream &stream, const ComicFavoriteUpdate &update)
{
    return stream << update.libraryId << update.comicId << update.favorite;
}

QDataStream &YACReader::operator>>(QDataStream &stream, ComicFavoriteUpdate &update)
{
    return stream >> update.libraryId >> update.comicId >> update.favorite;
}

QDataStream &YACReader::operator<<(QDataStream &stream, const ComicInfoAnswer &answer)
{
    stream << answer.comic << answer.siblings;
    if (answer.acceptsRatingAndFavorites)
        stream << RatingAndFavoritesMagic << answer.favoriteComicIds;
    return stream;
}

QDataStream &YACReader::operator>>(QDataStream &stream, ComicInfoAnswer &answer)
{
    stream >> answer.comic >> answer.siblings;
    answer.acceptsRatingAndFavorites = false;
    answer.favoriteComicIds.clear();
    if (stream.atEnd())
        return stream;

    // anything else after the siblings is a field this reader does not know
    quint32 magic = 0;
    stream >> magic;
    if (magic != RatingAndFavoritesMagic)
        return stream;

    QSet<qulonglong> favoriteComicIds;
    stream >> favoriteComicIds;
    if (stream.status() == QDataStream::Ok) {
        answer.acceptsRatingAndFavorites = true;
        answer.favoriteComicIds = favoriteComicIds;
    }
    return stream;
}

std::optional<QByteArray> YACReader::framedFields(const QByteArray &received)
{
    if (received.size() < qsizetype(sizeof(quint32)))
        return std::nullopt;

    QDataStream sizeStream(received);
    sizeStream.setVersion(QDataStream::Qt_4_8);
    quint32 size = 0;
    sizeStream >> size;
    if (received.size() - qsizetype(sizeof(quint32)) < qsizetype(size))
        return std::nullopt;

    return received.mid(sizeof(quint32), size);
}

QByteArray YACReader::comicInfoRequest(quint64 libraryId, const OpenComicSource &source, const ComicDB &comic)
{
    return framed(quint8(RequestComicInfo), libraryId, source, comic);
}
