#ifndef YACREADER_LOCAL_SERVER_H
#define YACREADER_LOCAL_SERVER_H

#include "yacreader_ipc.h"

#include <QMutex>
#include <QObject>
#include <QSet>
#include <QThread>

class QLocalServer;
class QLocalSocket;
class ComicDB;

class YACReaderLocalServer : public QObject
{
    Q_OBJECT
public:
    // only the tests change serverName, so that they do not take the place of a running YACReaderLibrary
    explicit YACReaderLocalServer(QObject *parent = nullptr, const QString &serverName = YACREADERLIBRARY_GUID);

signals:
    void comicUpdated(quint64 libraryId, const ComicDB &comic);
    void comicFavoriteChanged(quint64 libraryId, qulonglong comicId);
public slots:
    bool isListening();
    void sendResponse();
    static bool isRunning();
    void close();

private:
    // void run();
    QLocalServer *localServer;
};

class YACReaderClientConnectionWorker : public QThread
{
    Q_OBJECT
public:
    YACReaderClientConnectionWorker(QLocalSocket *clientConnection);
    ~YACReaderClientConnectionWorker() override;
signals:
    void comicUpdated(quint64 libraryId, const ComicDB &comic);
    void comicFavoriteChanged(quint64 libraryId, qulonglong comicId);

private:
    static QMutex dbMutex;
    // static int count;
    void run() override;

    void getComicInfo(quint64 libraryId, ComicDB &comic, QList<ComicDB> &siblings);
    void getComicInfoFromReadingList(quint64 libraryId, unsigned long long readingListId, ComicDB &comic, QList<ComicDB> &siblings);
    QSet<qulonglong> getFavoriteComicIds(quint64 libraryId);
    void updateComic(quint64 libraryId, ComicDB &comic);
    void updateComic(quint64 libraryId, ComicDB &comic, qulonglong nextComicId);
    // false for a message that is not SendComicRating or SendComicFavorite, or that the library did not save
    bool saveChange(quint8 messageType, QDataStream &payload);
    bool setComicRating(const YACReader::ComicRatingUpdate &update);
    bool setComicFavorite(const YACReader::ComicFavoriteUpdate &update);
    void acknowledge(bool saved);

    QLocalSocket *clientConnection;
};

#endif // YACREADER_LOCAL_SERVER_H
