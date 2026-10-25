#ifndef YACREADER_LOCAL_CLIENT_H
#define YACREADER_LOCAL_CLIENT_H

#include "comic_db.h"
#include "yacreader_global.h"

#include <QObject>
#include <QSet>

#include <optional>

class QLocalSocket;

class YACReaderLocalClient : public QObject
{
    Q_OBJECT
public:
    explicit YACReaderLocalClient(QObject *parent = nullptr);
    ~YACReaderLocalClient() override;
signals:
    void finished();
public slots:
    void readMessage();
    bool requestComicInfo(quint64 libraryId, ComicDB &comic, QList<ComicDB> &siblings, std::optional<QSet<qulonglong>> &favoriteComicIds, YACReader::OpenComicSource source);
    bool sendComicInfo(quint64 libraryId, ComicDB &comic);
    bool sendComicInfo(quint64 libraryId, ComicDB &comic, qulonglong nextComicId);
    bool sendComicRating(const YACReader::ComicRatingUpdate &update);
    bool sendComicFavorite(const YACReader::ComicFavoriteUpdate &update);

private:
    bool send(const QByteArray &message);

    QLocalSocket *localSocket;
};

#endif // YACREADER_LOCAL_CLIENT_H
