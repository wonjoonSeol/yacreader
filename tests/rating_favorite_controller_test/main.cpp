#include "rating_favorite_controller.h"
#include "yacreader_global_gui.h"
#include "yacreader_ipc.h"

#include <QAction>
#include <QApplication>
#include <QHash>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QMutex>
#include <QPixmap>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QThread>
#include <QWidget>

#include <memory>

using namespace YACReader;

namespace {
QString serverName()
{
    return QString("rating_favorite_controller_test-%1").arg(QCoreApplication::applicationPid());
}

struct Message {
    quint8 type = 0;
    quint64 libraryId = 0;
    qulonglong comicId = 0;
    int value = 0; // the rating, or 1 for a favorite

    bool operator==(const Message &other) const = default;
};

char *toString(const Message &message)
{
    return QTest::toString(QString("{type %1, library %2, comic %3, value %4}").arg(message.type).arg(message.libraryId).arg(message.comicId).arg(message.value));
}

ComicDB comicWithRating(qulonglong id, int rating)
{
    ComicDB comic;
    comic.id = id;
    comic.info.rating = rating;
    return comic;
}

ComicInfoAnswer libraryAnswer(bool acceptsChanges, const QSet<qulonglong> &favoriteComicIds, const ComicDB &comic = ComicDB())
{
    return { comic, { }, acceptsChanges, favoriteComicIds };
}
}

// Plays YACReaderLibrary: records the messages it gets and answers them as the test says
class FakeLibrary : public QObject
{
    Q_OBJECT
public:
    enum class ChangeAnswer {
        Saved,
        NotSaved,
        Never,
        Held
    };

    ChangeAnswer changeAnswer = ChangeAnswer::Saved;
    bool answerQuestions = true;
    ComicInfoAnswer questionAnswer;

    void listen()
    {
        server = new QLocalServer(this);
        QLocalServer::removeServer(serverName());
        server->listen(serverName());
        connect(server, &QLocalServer::newConnection, this, [this] {
            while (auto *connection = server->nextPendingConnection()) {
                connect(connection, &QLocalSocket::readyRead, this, [this, connection] { read(connection); });
                if (connection->bytesAvailable() > 0)
                    read(connection);
            }
        });
    }

    QList<Message> messages() const
    {
        QMutexLocker locker(&mutex);
        return received;
    }

    // answers the oldest change held back with ChangeAnswer::Held
    void answerHeldChange(char reply = ChangeSaved)
    {
        QPointer<QLocalSocket> connection = held.takeFirst();
        if (connection)
            connection->putChar(reply);
    }

private:
    void read(QLocalSocket *connection)
    {
        buffers[connection] += connection->readAll();
        const auto fields = framedFields(buffers[connection]);
        if (!fields.has_value())
            return;

        QDataStream stream(*fields);
        stream.setVersion(QDataStream::Qt_4_8);
        Message message;
        stream >> message.type;
        if (message.type == SendComicRating) {
            ComicRatingUpdate update;
            stream >> update;
            message = { message.type, update.libraryId, update.comicId, update.rating };
        } else if (message.type == SendComicFavorite) {
            ComicFavoriteUpdate update;
            stream >> update;
            message = { message.type, update.libraryId, update.comicId, update.favorite };
        } else if (message.type == RequestComicInfo) {
            OpenComicSource source;
            ComicDB comic;
            stream >> message.libraryId >> source >> comic;
            message.comicId = comic.id;
        }
        {
            QMutexLocker locker(&mutex);
            received << message;
        }

        if (message.type == RequestComicInfo) {
            if (answerQuestions)
                connection->write(framed(questionAnswer));
            else
                connection->abort();
            return;
        }
        switch (changeAnswer) {
        case ChangeAnswer::Saved:
            connection->putChar(ChangeSaved);
            break;
        case ChangeAnswer::NotSaved:
            connection->putChar(ChangeNotSaved);
            break;
        case ChangeAnswer::Never:
            break;
        case ChangeAnswer::Held:
            held << connection;
            break;
        }
    }

    QLocalServer *server = nullptr;
    QHash<QLocalSocket *, QByteArray> buffers;
    QList<QPointer<QLocalSocket>> held;
    mutable QMutex mutex;
    QList<Message> received;
};

// The reader window with the toolbar copies of the actions
struct Reader {
    explicit Reader(int requestTimeoutMs = 5000)
        : controller(new RatingFavoriteController(&window, serverName(), requestTimeoutMs))
    {
        QPixmap filled(18, 18);
        filled.fill(Qt::yellow);
        QPixmap empty(18, 18);
        empty.fill(Qt::gray);
        filledStar = QIcon(filled);
        emptyStar = QIcon(empty);

        toolbarActions.emplace_back(wrappedToolbarAction(controller->favoriteAction()));
        for (auto *rateAction : controller->ratingActions())
            toolbarActions.emplace_back(wrappedToolbarAction(rateAction));
        controller->setStarIcons(filledStar, filledStar, emptyStar);
    }

    QAction *heart() const { return toolbarActions.front().get(); }

    // the toolbar stars, filled up to the rating shown
    int filledStars() const
    {
        int stars = 0;
        for (auto toolbarAction = toolbarActions.cbegin() + 1; toolbarAction != toolbarActions.cend(); ++toolbarAction)
            stars += (*toolbarAction)->icon().cacheKey() == filledStar.cacheKey() ? 1 : 0;
        return stars;
    }

    bool buttonsEnabled() const
    {
        bool enabled = controller->ratingMenu()->menuAction()->isEnabled();
        for (const auto &toolbarAction : toolbarActions)
            enabled = enabled && toolbarAction->isEnabled();
        return enabled;
    }

    bool buttonsDisabled() const
    {
        bool disabled = !controller->ratingMenu()->menuAction()->isEnabled();
        for (const auto &toolbarAction : toolbarActions)
            disabled = disabled && !toolbarAction->isEnabled();
        return disabled;
    }

    void rate(int rating) { controller->ratingActions().at(rating - 1)->trigger(); }

    // destroyed after the window, whose actions update them while they live
    std::vector<std::unique_ptr<QAction>> toolbarActions;
    QIcon filledStar;
    QIcon emptyStar;
    QWidget window;
    RatingFavoriteController *controller;
};

class RatingFavoriteControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void aChangeShowsAtOnceAndReachesTheLibrary();
    void selectingTheCurrentRatingClearsIt();
    void oneRequestAtATimeSendsTheLatestChange();
    void aRefusedChangeShowsWhatTheLibraryHas();
    void aChangeThatTimesOutShowsWhatTheLibraryHas();
    void aFailedChangeAsksAboutTheComicOnScreen();
    void whenTheLibraryCannotSayWhatItHasTheButtonsAreDisabled();
    void anOlderLibraryLeavesTheButtonsDisabled();
    void filesOpenedDirectlyDisableTheButtons();
    void changesOnTheirWayAreSentBeforeQuitting();

private:
    std::unique_ptr<FakeLibrary> library;
};

void RatingFavoriteControllerTest::init()
{
    library = std::make_unique<FakeLibrary>();
    library->listen();
}

void RatingFavoriteControllerTest::cleanup()
{
    library.reset();
}

void RatingFavoriteControllerTest::aChangeShowsAtOnceAndReachesTheLibrary()
{
    Reader reader;
    reader.controller->setLibrary(4, { OpenComicSource::Folder, 1 }, libraryAnswer(true, { }));
    reader.controller->showComic(comicWithRating(7, 2));
    QVERIFY(reader.buttonsEnabled());
    QCOMPARE(reader.filledStars(), 2);
    QVERIFY(!reader.heart()->isChecked());
    QSignalSpy ratingChanged(reader.controller, &RatingFavoriteController::ratingChanged);

    reader.rate(4);
    reader.controller->favoriteAction()->trigger();

    // before any answer
    QCOMPARE(reader.filledStars(), 4);
    QVERIFY(reader.heart()->isChecked());
    QCOMPARE(ratingChanged.count(), 1);
    QCOMPARE(ratingChanged.at(0), (QList<QVariant> { 7ULL, 4 }));

    QTRY_COMPARE(library->messages().size(), 2);
    QCOMPARE(library->messages().at(0), (Message { SendComicRating, 4, 7, 4 }));
    QCOMPARE(library->messages().at(1), (Message { SendComicFavorite, 4, 7, 1 }));
    QTest::qWait(50);
    QCOMPARE(reader.filledStars(), 4);
    QVERIFY(reader.heart()->isChecked());
}

void RatingFavoriteControllerTest::selectingTheCurrentRatingClearsIt()
{
    Reader reader;
    reader.controller->setLibrary(4, { OpenComicSource::Folder, 1 }, libraryAnswer(true, { }));
    reader.controller->showComic(comicWithRating(7, 3));

    reader.rate(3);

    QCOMPARE(reader.filledStars(), 0);
    QTRY_COMPARE(library->messages().size(), 1);
    QCOMPARE(library->messages().at(0), (Message { SendComicRating, 4, 7, 0 }));
}

void RatingFavoriteControllerTest::oneRequestAtATimeSendsTheLatestChange()
{
    library->changeAnswer = FakeLibrary::ChangeAnswer::Held;
    Reader reader;
    reader.controller->setLibrary(4, { OpenComicSource::Folder, 1 }, libraryAnswer(true, { }));
    reader.controller->showComic(comicWithRating(7, 0));

    reader.rate(1);
    reader.rate(2);
    reader.rate(3);
    reader.controller->favoriteAction()->trigger();
    reader.controller->favoriteAction()->trigger();
    QCOMPARE(reader.filledStars(), 3);
    QVERIFY(!reader.heart()->isChecked());

    QTRY_COMPARE(library->messages().size(), 1);
    QTest::qWait(100);
    QCOMPARE(library->messages().size(), 1);
    QCOMPARE(library->messages().at(0), (Message { SendComicRating, 4, 7, 1 }));

    // only the latest of the queued ratings, then the heart as it ended up
    library->answerHeldChange();
    QTRY_COMPARE(library->messages().size(), 2);
    QCOMPARE(library->messages().at(1), (Message { SendComicRating, 4, 7, 3 }));
    library->answerHeldChange();
    QTRY_COMPARE(library->messages().size(), 3);
    QCOMPARE(library->messages().at(2), (Message { SendComicFavorite, 4, 7, 0 }));
    library->answerHeldChange();
    QTest::qWait(100);
    QCOMPARE(library->messages().size(), 3);
    QCOMPARE(reader.filledStars(), 3);
    QVERIFY(!reader.heart()->isChecked());
}

void RatingFavoriteControllerTest::aRefusedChangeShowsWhatTheLibraryHas()
{
    library->changeAnswer = FakeLibrary::ChangeAnswer::NotSaved;
    library->questionAnswer = libraryAnswer(true, { 7 }, comicWithRating(7, 1));
    Reader reader;
    reader.controller->setLibrary(4, { OpenComicSource::ReadingList, 9 }, libraryAnswer(true, { }));
    reader.controller->showComic(comicWithRating(7, 2));
    QSignalSpy ratingChanged(reader.controller, &RatingFavoriteController::ratingChanged);
    QSignalSpy changeNotSaved(reader.controller, &RatingFavoriteController::changeNotSaved);

    reader.rate(5);
    QCOMPARE(reader.filledStars(), 5);

    QTRY_COMPARE(reader.filledStars(), 1);
    QVERIFY(reader.heart()->isChecked());
    QVERIFY(reader.buttonsEnabled());
    QCOMPARE(library->messages(), (QList<Message> { { SendComicRating, 4, 7, 5 }, { RequestComicInfo, 4, 7, 0 } }));
    QCOMPARE(changeNotSaved.count(), 1);
    QCOMPARE(changeNotSaved.at(0).at(0).toString(), reader.controller->ratingMenu()->title());
    QCOMPARE(ratingChanged.last(), (QList<QVariant> { 7ULL, 1 }));
}

void RatingFavoriteControllerTest::aChangeThatTimesOutShowsWhatTheLibraryHas()
{
    library->changeAnswer = FakeLibrary::ChangeAnswer::Never;
    library->questionAnswer = libraryAnswer(true, { }, comicWithRating(7, 2));
    Reader reader(300);
    reader.controller->setLibrary(4, { OpenComicSource::Folder, 1 }, libraryAnswer(true, { }));
    reader.controller->showComic(comicWithRating(7, 2));
    QSignalSpy changeNotSaved(reader.controller, &RatingFavoriteController::changeNotSaved);

    reader.controller->favoriteAction()->trigger();
    QVERIFY(reader.heart()->isChecked());

    QTRY_VERIFY(!reader.heart()->isChecked());
    QCOMPARE(library->messages(), (QList<Message> { { SendComicFavorite, 4, 7, 1 }, { RequestComicInfo, 4, 7, 0 } }));
    QCOMPARE(changeNotSaved.count(), 1);
    QCOMPARE(changeNotSaved.at(0).at(0).toString(), reader.controller->favoriteAction()->text());
    QVERIFY(reader.buttonsEnabled());
}

void RatingFavoriteControllerTest::aFailedChangeAsksAboutTheComicOnScreen()
{
    library->changeAnswer = FakeLibrary::ChangeAnswer::Held;
    library->questionAnswer = libraryAnswer(true, { }, comicWithRating(8, 3));
    Reader reader;
    reader.controller->setLibrary(4, { OpenComicSource::Folder, 1 }, libraryAnswer(true, { }));
    reader.controller->showComic(comicWithRating(7, 0));
    reader.rate(4);
    QTRY_COMPARE(library->messages().size(), 1);

    // the reader moved on to the next comic before the library refused the rating of the first
    reader.controller->showComic(comicWithRating(8, 0));
    library->answerHeldChange(ChangeNotSaved);

    QTRY_COMPARE(library->messages().size(), 2);
    QCOMPARE(library->messages().at(1), (Message { RequestComicInfo, 4, 8, 0 }));
    QTRY_COMPARE(reader.filledStars(), 3);
}

void RatingFavoriteControllerTest::whenTheLibraryCannotSayWhatItHasTheButtonsAreDisabled()
{
    library->changeAnswer = FakeLibrary::ChangeAnswer::NotSaved;
    library->answerQuestions = false;
    Reader reader;
    reader.controller->setLibrary(4, { OpenComicSource::Folder, 1 }, libraryAnswer(true, { }));
    reader.controller->showComic(comicWithRating(7, 2));

    reader.controller->favoriteAction()->trigger();

    QTRY_VERIFY(reader.buttonsDisabled());
    QVERIFY(!reader.heart()->isChecked());
    QCOMPARE(reader.filledStars(), 0);
    QCOMPARE(library->messages().size(), 2);
}

void RatingFavoriteControllerTest::anOlderLibraryLeavesTheButtonsDisabled()
{
    Reader reader;
    reader.controller->setLibrary(4, { OpenComicSource::Folder, 1 }, libraryAnswer(false, { }));
    reader.controller->showComic(comicWithRating(7, 2));

    QVERIFY(reader.buttonsDisabled());
    QCOMPARE(reader.filledStars(), 0);
    reader.rate(4);
    reader.controller->favoriteAction()->trigger();
    QTest::qWait(100);
    QVERIFY(library->messages().isEmpty());
}

void RatingFavoriteControllerTest::filesOpenedDirectlyDisableTheButtons()
{
    Reader reader;
    reader.controller->setLibrary(4, { OpenComicSource::Folder, 1 }, libraryAnswer(true, { 7 }));
    reader.controller->showComic(comicWithRating(7, 2));
    QVERIFY(reader.heart()->isChecked());

    reader.controller->showNoComic();

    QVERIFY(reader.buttonsDisabled());
    QVERIFY(!reader.heart()->isChecked());
    QCOMPARE(reader.filledStars(), 0);
}

void RatingFavoriteControllerTest::changesOnTheirWayAreSentBeforeQuitting()
{
    // the reader waits without its event loop, so the library needs its own thread
    library.reset();
    QThread libraryThread;
    auto *threadLibrary = new FakeLibrary;
    threadLibrary->moveToThread(&libraryThread);
    connect(&libraryThread, &QThread::finished, threadLibrary, &QObject::deleteLater);
    libraryThread.start();
    const auto stopLibrary = qScopeGuard([&libraryThread] {
        libraryThread.quit();
        libraryThread.wait();
    });
    QMetaObject::invokeMethod(threadLibrary, &FakeLibrary::listen, Qt::BlockingQueuedConnection);

    Reader reader;
    reader.controller->setLibrary(4, { OpenComicSource::Folder, 1 }, libraryAnswer(true, { }));
    reader.controller->showComic(comicWithRating(7, 0));
    reader.controller->favoriteAction()->trigger();
    reader.rate(4);

    reader.controller->sendQueuedChanges();

    QCOMPARE(threadLibrary->messages(), (QList<Message> { { SendComicFavorite, 4, 7, 1 }, { SendComicRating, 4, 7, 4 } }));
}

int main(int argc, char *argv[])
{
    // the actions need a QApplication; ctest runs without a display
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    RatingFavoriteControllerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "main.moc"
