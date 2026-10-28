#include "comic_db.h"
#include "comics_remover.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QSignalSpy>
#include <QStringListModel>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <memory>

class ComicsDeletionTest : public QObject
{
    Q_OBJECT

private slots:
    void plansFolderComicsOnlyWithTheirFolders();
    void movesComicsToTheTrash();
    void keepsTheFoldersTheTrashRefuses();
    void deletesOnlyThePagesOfFoldersForGood();
    void reportsWhichComicsCouldNotBeDeleted();
    void leavesAFolderThatIsNoLongerAComic();
    void deletesOnlyTheLinkOfALinkedFolderForGood();
};

namespace {

bool writeFile(const QString &path, const QByteArray &content = "data")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

bool writeFolderComic(const QString &folder, int pages)
{
    for (int page = 1; page <= pages; ++page) {
        if (!writeFile(QDir(folder).filePath(QStringLiteral("%1.png").arg(page)))) {
            return false;
        }
    }
    return true;
}

ComicDB comicAt(const QString &path, int numPages)
{
    ComicDB comic;
    comic.path = path;
    comic.info.numPages = numPages;
    return comic;
}

// runs a remover over the given paths, with rows that match their positions after firstRow rows
struct RemoverRun {
    explicit RemoverRun(const QStringList &paths, ComicsRemover::FolderDeletion folderDeletion, int firstRow = 0)
        : model(QStringList(firstRow, QString()) + paths), paths(paths)
    {
        for (int position = 0; position < paths.size(); ++position) {
            indexes.append(model.index(firstRow + position));
        }
        remover = std::make_unique<ComicsRemover>(indexes, this->paths, 1, folderDeletion);
        removed = std::make_unique<QSignalSpy>(remover.get(), &ComicsRemover::remove);
        failed = std::make_unique<QSignalSpy>(remover.get(), &ComicsRemover::removeError);
        notTrashed = std::make_unique<QSignalSpy>(remover.get(), &ComicsRemover::folderNotTrashed);
        notEmptied = std::make_unique<QSignalSpy>(remover.get(), &ComicsRemover::folderNotEmptied);
        deletedForGood = std::make_unique<QSignalSpy>(remover.get(), &ComicsRemover::fileDeletedPermanently);
    }

    QList<int> spied(const QSignalSpy &spy) const
    {
        QList<int> values;
        for (const auto &arguments : spy) {
            values.append(arguments.at(0).toInt());
        }
        std::sort(values.begin(), values.end());
        return values;
    }

    QStringListModel model;
    QModelIndexList indexes;
    QList<QString> paths;
    std::unique_ptr<ComicsRemover> remover;
    std::unique_ptr<QSignalSpy> removed;
    std::unique_ptr<QSignalSpy> failed;
    std::unique_ptr<QSignalSpy> notTrashed;
    std::unique_ptr<QSignalSpy> notEmptied;
    std::unique_ptr<QSignalSpy> deletedForGood;
};

}

void ComicsDeletionTest::plansFolderComicsOnlyWithTheirFolders()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeFolderComic(root.filePath("Chapter 1"), 2));
    QVERIFY(writeFile(root.filePath("Book.cbz")));
    QVERIFY(writeFolderComic(root.filePath("Chapter 2"), 3));

    // page counts come from the library, not from listing the folders again
    const ComicsDeletion deletion({ comicAt("/Chapter 1", 2), comicAt("/Book.cbz", 20), comicAt("/Chapter 2", 7), comicAt("/Missing.cbz", 5) }, root.path());

    QCOMPARE(deletion.files, QList<int>({ 1, 3 }));
    QCOMPARE(deletion.folders, QList<int>({ 0, 2 }));
    QCOMPARE(deletion.folderImages, 9);
    QCOMPARE(deletion.deleted(false), QList<int>({ 1, 3 }));
    QCOMPARE(deletion.deleted(true), QList<int>({ 1, 3, 0, 2 }));
}

void ComicsDeletionTest::movesComicsToTheTrash()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeFolderComic(root.filePath("Chapter"), 2));
    QVERIFY(writeFile(root.filePath("Book.cbz")));

    RemoverRun run({ root.filePath("Chapter"), root.filePath("Book.cbz") }, ComicsRemover::FolderDeletion::ToTrash);
    QStringList trashed;
    run.remover->setTrash([&trashed](const QString &path) {
        trashed.append(QFileInfo(path).fileName());
        return true;
    });

    run.remover->process();

    QCOMPARE(trashed, QStringList({ "Book.cbz", "Chapter" }));
    QCOMPARE(run.spied(*run.removed), QList<int>({ 0, 1 }));
    QCOMPARE(run.failed->count(), 0);
    QCOMPARE(run.notTrashed->count(), 0);
}

void ComicsDeletionTest::keepsTheFoldersTheTrashRefuses()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeFile(root.filePath("Book.cbz")));
    QVERIFY(writeFolderComic(root.filePath("Chapter"), 2));

    RemoverRun run({ root.filePath("Book.cbz"), root.filePath("Chapter") }, ComicsRemover::FolderDeletion::ToTrash);
    run.remover->setTrash([](const QString &) { return false; });

    run.remover->process();

    QCOMPARE(run.spied(*run.notTrashed), QList<int>({ 1 }));
    QCOMPARE(QDir(root.filePath("Chapter")).entryList(QDir::Files).size(), 2);
    // a comic file the trash refuses is deleted for good, as it always was
    QCOMPARE(run.deletedForGood->count(), 1);
    QVERIFY(!QFileInfo::exists(root.filePath("Book.cbz")));
    QCOMPARE(run.spied(*run.removed), QList<int>({ 0 }));
}

void ComicsDeletionTest::deletesOnlyThePagesOfFoldersForGood()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeFolderComic(root.filePath("Only pages"), 2));
    QVERIFY(writeFile(root.filePath("Only pages/ComicInfo.xml")));
    // what Windows and macOS leave behind goes with the pages
    for (const auto &name : { ".DS_Store", "._1.png", "Thumbs.db", "Desktop.ini" }) {
        QVERIFY(writeFile(root.filePath("Only pages/") + name));
    }
    QVERIFY(writeFolderComic(root.filePath("With notes"), 3));
    QVERIFY(writeFile(root.filePath("With notes/notes.txt")));
    QVERIFY(writeFile(root.filePath("With notes/Thumbs.db")));
    QVERIFY(writeFile(root.filePath("With notes/@eaDir/1.png/SYNOPHOTO_THUMB_M.jpg")));

    RemoverRun run({ root.filePath("Only pages"), root.filePath("With notes") }, ComicsRemover::FolderDeletion::Permanently);
    run.remover->setTrash([](const QString &path) {
        qWarning("trash asked for %s", qPrintable(path));
        return false;
    });

    QTest::failOnWarning("trash asked for");
    run.remover->process();

    QVERIFY(!QFileInfo::exists(root.filePath("Only pages")));
    QCOMPARE(QDir(root.filePath("With notes")).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot), QStringList({ "@eaDir", "notes.txt" }));
    QCOMPARE(run.spied(*run.removed), QList<int>({ 0, 1 }));
    QCOMPARE(run.notEmptied->count(), 1);
    QCOMPARE(run.notEmptied->at(0).at(0).toInt(), 1);
    QCOMPARE(run.notEmptied->at(0).at(1).toStringList(), QStringList({ "@eaDir", "notes.txt" }));
    QCOMPARE(run.failed->count(), 0);
}

void ComicsDeletionTest::reportsWhichComicsCouldNotBeDeleted()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeFile(root.filePath("Book.cbz")));
    QVERIFY(writeFolderComic(root.filePath("Chapter"), 2));

    // rows in the model and positions in the list differ, the coordinator names the comic by its position
    RemoverRun run({ root.filePath("Missing.cbz"), root.filePath("Book.cbz"), root.filePath("Chapter") }, ComicsRemover::FolderDeletion::ToTrash, 5);
    run.remover->setTrash([](const QString &path) { return QFileInfo::exists(path); });

    run.remover->process();

    QCOMPARE(run.spied(*run.failed), QList<int>({ 0 }));
    QCOMPARE(run.spied(*run.removed), QList<int>({ 6, 7 }));
}

void ComicsDeletionTest::leavesAFolderThatIsNoLongerAComic()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeFolderComic(root.filePath("Chapter"), 2));
    QVERIFY(writeFile(root.filePath("Chapter/Extras/1.png")));
    QVERIFY(writeFolderComic(root.filePath("Volume"), 2));
    QVERIFY(writeFile(root.filePath("Volume/Book.cbz")));

    for (const auto folderDeletion : { ComicsRemover::FolderDeletion::ToTrash, ComicsRemover::FolderDeletion::Permanently }) {
        RemoverRun run({ root.filePath("Chapter"), root.filePath("Volume") }, folderDeletion);
        run.remover->setTrash([](const QString &) { return true; });

        run.remover->process();

        QCOMPARE(run.failed->count(), 2);
        QCOMPARE(run.removed->count(), 0);
        QCOMPARE(QDir(root.filePath("Chapter")).entryList(QDir::Files).size(), 2);
        QCOMPARE(QDir(root.filePath("Volume")).entryList(QDir::Files).size(), 3);
    }
}

void ComicsDeletionTest::deletesOnlyTheLinkOfALinkedFolderForGood()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeFolderComic(root.filePath("Elsewhere/Chapter"), 2));
    QVERIFY(QDir().mkpath(root.filePath("Library")));
    const auto link = root.filePath("Library/Chapter");
#ifdef Q_OS_WIN
    // QFile::link makes a .lnk shortcut on Windows, and a junction needs no special rights, unlike a symbolic link
    QProcess::execute("cmd.exe", { "/c", "mklink", "/J", QDir::toNativeSeparators(link), QDir::toNativeSeparators(root.filePath("Elsewhere/Chapter")) });
#else
    QFile::link(root.filePath("Elsewhere/Chapter"), link);
#endif
    QVERIFY2(QFileInfo(link).isSymbolicLink() || QFileInfo(link).isJunction(), qPrintable(link));

    RemoverRun run({ link }, ComicsRemover::FolderDeletion::Permanently);

    run.remover->process();

    QCOMPARE(run.spied(*run.removed), QList<int>({ 0 }));
    QVERIFY(!QFileInfo::exists(link));
    QCOMPARE(QDir(root.filePath("Elsewhere/Chapter")).entryList(QDir::Files).size(), 2);
}

QTEST_GUILESS_MAIN(ComicsDeletionTest)

#include "main.moc"
