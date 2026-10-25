#include "comics_remover.h"

#include "QsLog.h"
#include "comic.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

ComicsDeletion::ComicsDeletion(const QList<ComicDB> &comics, const QString &libraryPath)
{
    for (int position = 0; position < comics.size(); ++position) {
        // only a stat here, ComicsRemover checks that a folder is still a comic before deleting it
        if (QFileInfo(libraryPath + comics.at(position).path).isDir()) {
            folders.append(position);
            folderImages += comics.at(position).info.numPages.toInt();
        } else {
            files.append(position);
        }
    }
}

QList<int> ComicsDeletion::deleted(bool deleteFolders) const
{
    if (!deleteFolders) {
        return files;
    }
    auto positions = files + folders;
    std::sort(positions.begin(), positions.end());
    return positions;
}

ComicsRemover::ComicsRemover(QModelIndexList &il, QList<QString> &ps, qulonglong parentId, FolderDeletion folderDeletion, QObject *parent)
    : QObject(parent), indexList(il), paths(ps), parentId(parentId), folderDeletion(folderDeletion)
{
}

void ComicsRemover::process()
{
    for (int position = indexList.size() - 1; position >= 0; --position) {
        const int row = indexList.at(position).row();
        const QString &path = paths.at(position);
        if (QFileInfo(path).isDir()) {
            removeFolder(position, row, path);
        } else {
            removeFile(row, path);
        }
    }

    emit finished();
    emit removedItemsFromFolder(parentId);
}

void ComicsRemover::removeFile(int row, const QString &path)
{
    if (trash(path)) {
        emit remove(row);
    } else if (QFile::remove(path)) {
        emit fileDeletedPermanently();
        emit remove(row);
    } else {
        emit removeError();
    }
}

void ComicsRemover::removeFolder(int position, int row, const QString &path)
{
    // a folder that holds more than a comic by now, like a subfolder or an archive, is left alone
    const auto contents = FolderComic::comicContents(path);
    if (!contents) {
        emit removeError();
        return;
    }

    if (folderDeletion == FolderDeletion::ToTrash) {
        if (trash(path)) {
            emit remove(row);
        } else {
            emit folderNotTrashed(position);
        }
        return;
    }

    switch (deletePermanently(path, *contents)) {
    case PermanentDeletion::Deleted:
        emit remove(row);
        break;
    case PermanentDeletion::FilesLeft:
        emit remove(row);
        emit folderNotEmptied(position);
        break;
    case PermanentDeletion::Failed:
        emit removeError();
        break;
    }
}

ComicsRemover::PermanentDeletion ComicsRemover::deletePermanently(const QString &path, const FolderComicContents &contents)
{
    const QFileInfo folder(path);
    if (folder.isSymbolicLink() || folder.isJunction()) {
        // only the link, not the folder it points to; rmdir is how Windows removes a link to a folder
        return QFile::remove(path) || QDir().rmdir(path) ? PermanentDeletion::Deleted : PermanentDeletion::Failed;
    }

    for (const QFileInfo &page : contents.pages) {
        if (!QFile::remove(page.absoluteFilePath())) {
            return PermanentDeletion::Failed;
        }
    }
    if (!contents.comicInfoPath.isEmpty()) {
        QFile::remove(contents.comicInfoPath);
    }
    return QDir().rmdir(path) ? PermanentDeletion::Deleted : PermanentDeletion::FilesLeft;
}

FoldersRemover::FoldersRemover(QModelIndexList &il, QList<QString> &ps, QObject *parent)
    : QObject(parent), indexList(il), paths(ps)
{
}

void FoldersRemover::process()
{
    QString currentFolderPath;
    QListIterator<QModelIndex> i(indexList);
    QListIterator<QString> i2(paths);
    i.toBack();
    i2.toBack();

    QLOG_DEBUG() << "Deleting folders" << paths.at(0);

    while (i.hasPrevious() && i2.hasPrevious()) {
        QModelIndex mi = i.previous();
        currentFolderPath = i2.previous();
        QDir d(currentFolderPath);
        if (QFile::moveToTrash(currentFolderPath))
            emit remove(mi);
        else if (d.removeRecursively() || !d.exists()) // the folder is in the DB but no in the drive...
            emit remove(mi);
        else
            emit removeError();
    }

    emit finished();
}
