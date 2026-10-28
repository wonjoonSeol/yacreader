#include "rating_favorite_controller.h"

#include "shortcuts_manager.h"

#include <QAction>
#include <QDeadlineTimer>
#include <QLocalSocket>
#include <QMenu>
#include <QWidget>

#include <algorithm>
#include <utility>

using namespace YACReader;

namespace {
QAction *addActionWithShortcut(QWidget *window, const QString &text, const QString &shortcutKey)
{
    auto *const action = new QAction(text, window);
    action->setData(shortcutKey);
    action->setShortcut(ShortcutsManager::getShortcutsManager().getShortcut(shortcutKey));
    action->setAutoRepeat(false);
    window->addAction(action);
    return action;
}
}

RatingFavoriteController::RatingFavoriteController(QWidget *window, const QString &serverName, int requestTimeoutMs)
    : QObject(window), serverName(serverName), requestTimeoutMs(requestTimeoutMs)
{
    toggleFavoriteAction = addActionWithShortcut(window, tr("Favorite"), TOGGLE_FAVORITE_ACTION_Y);
    toggleFavoriteAction->setToolTip(tr("Add the comic to the library favorites or remove it"));
    toggleFavoriteAction->setCheckable(true);
    connect(toggleFavoriteAction, &QAction::triggered, this, &RatingFavoriteController::setFavorite);

    const QStringList rateShortcutKeys { RATE_1_ACTION_Y, RATE_2_ACTION_Y, RATE_3_ACTION_Y, RATE_4_ACTION_Y, RATE_5_ACTION_Y };
    for (int rating = 1; rating <= MaxComicRating; rating++) {
        auto *const rateAction = addActionWithShortcut(window, tr("Rate %1 out of %2").arg(rating).arg(MaxComicRating), rateShortcutKeys[rating - 1]);
        connect(rateAction, &QAction::triggered, this, [this, rating] { rate(rating); });
        rateActions << rateAction;
    }
    rateMenu = new QMenu(tr("Rating"), window);
    rateMenu->addActions(rateActions);

    requestTimer.setSingleShot(true);
    connect(&requestTimer, &QTimer::timeout, this, &RatingFavoriteController::requestFailed);

    updateActions();
}

QAction *RatingFavoriteController::favoriteAction() const
{
    return toggleFavoriteAction;
}

const QList<QAction *> &RatingFavoriteController::ratingActions() const
{
    return rateActions;
}

QMenu *RatingFavoriteController::ratingMenu() const
{
    return rateMenu;
}

void RatingFavoriteController::setLibrary(quint64 id, const OpenComicSource &openSource, const ComicInfoAnswer &answer)
{
    libraryId = id;
    source = openSource;
    libraryAcceptsChanges = answer.acceptsRatingAndFavorites;
    favoriteComicIds = answer.favoriteComicIds;
    updateActions();
}

void RatingFavoriteController::showComic(const ComicDB &comic)
{
    shownComicId = comic.id;
    shownRating = comic.info.rating;
    updateActions();
}

void RatingFavoriteController::showNoComic()
{
    shownComicId.reset();
    updateActions();
}

void RatingFavoriteController::setStarIcons(const QIcon &star, const QIcon &star18x18, const QIcon &emptyStar18x18)
{
    rateMenu->menuAction()->setIcon(star);
    for (auto *rateAction : std::as_const(rateActions))
        rateAction->setIcon(star);
    starIcon18x18 = star18x18;
    emptyStarIcon18x18 = emptyStar18x18;
    updateActions();
}

void RatingFavoriteController::sendQueuedChanges()
{
    // nothing is shown any more, so a failed change is not asked about again
    quitting = true;
    comicToQuery.reset();
    if (socket != nullptr && !sentChange.has_value())
        endRequest();
    sendNextRequest();

    // the event loop no longer runs, so wait here; the handlers run inside the waits and start the next change
    const QDeadlineTimer deadline(requestTimeoutMs);
    while (socket != nullptr) {
        auto *const requestSocket = socket;
        const int remainingMs = int(deadline.remainingTime());
        if (requestSocket->state() == QLocalSocket::ConnectingState)
            requestSocket->waitForConnected(remainingMs);
        else if (requestSocket->bytesToWrite() > 0)
            requestSocket->waitForBytesWritten(remainingMs);
        else
            requestSocket->waitForReadyRead(remainingMs);

        if (requestSocket == socket && (deadline.hasExpired() || requestSocket->state() == QLocalSocket::UnconnectedState))
            requestFailed();
    }
}

void RatingFavoriteController::rate(int selectedRating)
{
    if (!libraryAcceptsChanges || !shownComicId.has_value())
        return;

    // selecting the current rating again clears it
    shownRating = selectedRating == shownRating ? 0 : selectedRating;
    updateActions();
    emit ratingChanged(*shownComicId, shownRating);
    queueChange({ Change::Rating, *shownComicId, shownRating });
}

void RatingFavoriteController::setFavorite(bool favorite)
{
    if (!libraryAcceptsChanges || !shownComicId.has_value())
        return;

    if (favorite)
        favoriteComicIds.insert(*shownComicId);
    else
        favoriteComicIds.remove(*shownComicId);
    updateActions();
    queueChange({ Change::Favorite, *shownComicId, favorite });
}

void RatingFavoriteController::queueChange(const Change &change)
{
    auto queued = std::find_if(queuedChanges.begin(), queuedChanges.end(), [&change](const Change &other) {
        return other.kind == change.kind && other.comicId == change.comicId;
    });
    if (queued != queuedChanges.end())
        queued->value = change.value;
    else
        queuedChanges.append(change);

    sendNextRequest();
}

void RatingFavoriteController::sendNextRequest()
{
    if (socket != nullptr)
        return;

    QByteArray message;
    if (comicToQuery.has_value()) {
        queriedComicId = *std::exchange(comicToQuery, std::nullopt);
        sentChange.reset();
        ComicDB comic;
        comic.id = queriedComicId;
        message = comicInfoRequest(libraryId, source, comic);
    } else if (!queuedChanges.isEmpty()) {
        const auto change = queuedChanges.takeFirst();
        sentChange = change;
        if (change.kind == Change::Rating)
            message = framed(quint8(SendComicRating), ComicRatingUpdate { libraryId, change.comicId, change.value });
        else
            message = framed(quint8(SendComicFavorite), ComicFavoriteUpdate { libraryId, change.comicId, change.value != 0 });
    } else {
        return;
    }

    startRequest(message);
}

void RatingFavoriteController::startRequest(const QByteArray &message)
{
    received.clear();
    socket = new QLocalSocket(this);
    auto *const requestSocket = socket;
    connect(requestSocket, &QLocalSocket::connected, this, [requestSocket, message] { requestSocket->write(message); });
    connect(requestSocket, &QLocalSocket::readyRead, this, [this, requestSocket] {
        if (requestSocket == socket)
            readReply();
    });
    // the answer can come with the end of the connection, so read it first
    const auto failUnlessAnswered = [this, requestSocket] {
        if (requestSocket == socket)
            readReply();
        if (requestSocket == socket)
            requestFailed();
    };
    connect(requestSocket, &QLocalSocket::disconnected, this, failUnlessAnswered);
    connect(requestSocket, &QLocalSocket::errorOccurred, this, failUnlessAnswered);

    requestTimer.start(requestTimeoutMs);
    // this can fail at once and start the next request, so nothing may follow it
    requestSocket->connectToServer(serverName);
}

void RatingFavoriteController::readReply()
{
    received += socket->readAll();

    if (sentChange.has_value()) {
        if (received.isEmpty())
            return;
        if (received.at(0) != ChangeSaved) {
            requestFailed();
            return;
        }
        sentChange.reset();
        endRequest();
        sendNextRequest();
        return;
    }

    const auto fields = framedFields(received);
    if (!fields.has_value())
        return;
    QDataStream stream(*fields);
    stream.setVersion(QDataStream::Qt_4_8);
    ComicInfoAnswer answer;
    stream >> answer;
    endRequest();
    applyAnswer(answer);
    sendNextRequest();
}

// a change the library refused or did not answer in time, or a question it did not answer
void RatingFavoriteController::requestFailed()
{
    const auto failedChange = std::exchange(sentChange, std::nullopt);
    endRequest();
    // they were made on top of a state the library may not have
    queuedChanges.clear();

    if (!failedChange.has_value()) {
        // the library cannot say what it has, so show nothing rather than a guess
        libraryAcceptsChanges = false;
        updateActions();
        return;
    }
    if (quitting)
        return;

    // the library's state replaces the change on screen once it answers
    comicToQuery = shownComicId.value_or(failedChange->comicId);
    sendNextRequest();
    emit changeNotSaved(failedChange->kind == Change::Favorite ? toggleFavoriteAction->text() : rateMenu->title());
}

void RatingFavoriteController::endRequest()
{
    requestTimer.stop();
    if (socket == nullptr)
        return;

    socket->disconnect(this);
    socket->abort();
    socket->deleteLater();
    socket = nullptr;
}

void RatingFavoriteController::applyAnswer(const ComicInfoAnswer &answer)
{
    libraryAcceptsChanges = answer.acceptsRatingAndFavorites;
    if (!libraryAcceptsChanges)
        queuedChanges.clear();

    // changes made while the question was on its way still show, as they are sent next
    favoriteComicIds = answer.favoriteComicIds;
    bool ratingQueued = false;
    for (const auto &change : std::as_const(queuedChanges)) {
        if (change.kind == Change::Rating) {
            ratingQueued = ratingQueued || change.comicId == queriedComicId;
        } else if (change.value != 0) {
            favoriteComicIds.insert(change.comicId);
        } else {
            favoriteComicIds.remove(change.comicId);
        }
    }

    if (!ratingQueued) {
        const int rating = answer.comic.info.rating;
        if (shownComicId == queriedComicId)
            shownRating = rating;
        emit ratingChanged(queriedComicId, rating);
    }
    updateActions();
}

void RatingFavoriteController::updateActions()
{
    const bool available = libraryAcceptsChanges && shownComicId.has_value();
    toggleFavoriteAction->setEnabled(available);
    toggleFavoriteAction->setChecked(available && favoriteComicIds.contains(*shownComicId));
    rateMenu->menuAction()->setEnabled(available);

    const int rating = available ? shownRating : 0;
    for (int star = 1; star <= rateActions.size(); star++) {
        auto *const rateAction = rateActions.at(star - 1);
        rateAction->setEnabled(available);
        // the copy that YACReader::wrappedToolbarAction made for the toolbar
        if (auto *const toolbarAction = rateAction->property("wrappedToolbarAction").value<QAction *>())
            toolbarAction->setIcon(star <= rating ? starIcon18x18 : emptyStarIcon18x18);
    }
}
