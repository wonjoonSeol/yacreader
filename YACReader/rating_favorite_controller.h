#ifndef RATING_FAVORITE_CONTROLLER_H
#define RATING_FAVORITE_CONTROLLER_H

#include "yacreader_ipc.h"

#include <QIcon>
#include <QList>
#include <QObject>
#include <QSet>
#include <QTimer>

#include <optional>

class QAction;
class QLocalSocket;
class QMenu;
class QWidget;

// The favorite and rating actions for comics opened from the library. A change shows at once and goes to the
// library in the background, one request at a time. When the library does not save it, the reader asks the
// library again and shows what it has.
class RatingFavoriteController : public QObject
{
    Q_OBJECT
public:
    // More than the library can take for one change: each of its database connections waits up to 2 s for a lock
    // (QSQLITE_BUSY_TIMEOUT in DataBaseManagement::loadDatabase), after the database work of other connections.
    static constexpr int DefaultRequestTimeoutMs = 10000;

    // The actions are added to window, so their shortcuts work there. Only the tests change the other arguments.
    explicit RatingFavoriteController(QWidget *window, const QString &serverName = YACREADERLIBRARY_GUID, int requestTimeoutMs = DefaultRequestTimeoutMs);

    QAction *favoriteAction() const;
    const QList<QAction *> &ratingActions() const;
    QMenu *ratingMenu() const;

    // the library the comics come from, with its answer to RequestComicInfo
    void setLibrary(quint64 libraryId, const YACReader::OpenComicSource &source, const YACReader::ComicInfoAnswer &answer);
    // a comic of that library is on screen
    void showComic(const ComicDB &comic);
    // a file or folder opened directly, or no comic at all
    void showNoComic();
    // star is for the menu entries; the toolbar copies of the actions show star18x18 up to the rating and emptyStar18x18 after it
    void setStarIcons(const QIcon &star, const QIcon &star18x18, const QIcon &emptyStar18x18);
    // For the reader's close: sends the changes the library has not got yet, waiting at most the request timeout in all.
    void sendQueuedChanges();

signals:
    // the rating the reader shows for the comic, which the progress it sends on close has to keep
    void ratingChanged(qulonglong comicId, int rating);
    // title names what the library did not save
    void changeNotSaved(const QString &title);

private:
    // a change of the rating or the favorite state of a comic, or a question about what the library has for it
    struct Request {
        enum Kind {
            Rating,
            Favorite,
            Query
        } kind;
        qulonglong comicId;
        int value;
    };

    void rate(int selectedRating);
    void setFavorite(bool favorite);
    void queueChange(const Request &change);
    void sendNextChange();
    void startRequest(const Request &request, const QByteArray &message);
    void readReply();
    void requestFailed();
    void endRequest();
    void applyAnswer(qulonglong queriedComicId, const YACReader::ComicInfoAnswer &answer);
    void updateActions();

    QAction *toggleFavoriteAction;
    QList<QAction *> rateActions;
    QMenu *rateMenu;
    QIcon starIcon18x18;
    QIcon emptyStarIcon18x18;

    QString serverName;
    int requestTimeoutMs;
    quint64 libraryId = 0;
    YACReader::OpenComicSource source { YACReader::OpenComicSource::Folder, 0 };
    // false for libraries without SendComicRating and SendComicFavorite, and once the library could not say what it has
    bool libraryAcceptsChanges = false;
    // the library's favorites, with the changes it has not confirmed yet
    QSet<qulonglong> favoriteComicIds;
    std::optional<qulonglong> shownComicId;
    int shownRating = 0;

    // changes not sent yet, only the latest of each kind for each comic
    QList<Request> queuedChanges;
    // set while socket carries it
    std::optional<Request> inFlight;
    bool quitting = false;
    QLocalSocket *socket = nullptr;
    QByteArray received;
    QTimer requestTimer;
};

#endif // RATING_FAVORITE_CONTROLLER_H
