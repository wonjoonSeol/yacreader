#include "comics_remover.h"

#include "QsLog.h"
#include "comic.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

ComicsDeletion::ComicsDeletion(const QStringList &comicPaths)
{
    for (int position = 0; position < comicPaths.size(); ++position) {
        if (QFileInfo(comicPaths.at(position)).isDir()) {
            folders.append(position);
            folderImages += FolderComic::pageFiles(comicPaths.at(position)).size();
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

ComicsRemover::ComicsRemover(QModelIndexList &il, QList<QString> &ps, qulonglong parentId, QObject *parent)
    : QObject(parent), indexList(il), paths(ps), parentId(parentId)
{
}

void ComicsRemover::process()
{
    QString currentComicPath;
    QListIterator<QModelIndex> i(indexList);
    QListIterator<QString> i2(paths);
    i.toBack();
    i2.toBack();

    while (i.hasPrevious() && i2.hasPrevious()) {
        QModelIndex mi = i.previous();
        currentComicPath = i2.previous();
        if (QFileInfo(currentComicPath).isDir())
            removeFolder(mi.row(), currentComicPath);
        else if (QFile::moveToTrash(currentComicPath))
            emit remove(mi.row());
        else if (QFile::remove(currentComicPath))
            emit remove(mi.row());
        else
            emit removeError();
    }

    emit finished();
    emit removedItemsFromFolder(parentId);
}

void ComicsRemover::removeFolder(int row, const QString &path)
{
    if (deleteFoldersPermanently ? QDir(path).removeRecursively() : QFile::moveToTrash(path))
        emit remove(row);
    else if (deleteFoldersPermanently)
        emit removeError();
    else
        emit folderNotTrashed(path);
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
