#ifndef COMICS_REMOVER_H
#define COMICS_REMOVER_H

#include "comic_db.h"
#include "folder_comic_contents.h"

#include <QFile>
#include <QModelIndex>
#include <QThread>

#include <functional>

// What deleting some comics removes from disk: comic files always, folder comics only when
// their image folders are deleted too. Positions refer to the list of comics.
struct ComicsDeletion {
    ComicsDeletion(const QList<ComicDB> &comics, const QString &libraryPath);
    QList<int> deleted(bool deleteFolders) const;

    // comic files, and comics missing from disk, whose deletion fails and is reported as before
    QList<int> files;
    QList<int> folders;
    int folderImages = 0;
};

class ComicsRemover : public QObject
{
    Q_OBJECT
public:
    enum class FolderDeletion {
        // the whole folder, with everything in it
        ToTrash,
        // for the folders the trash refused: their pages, ComicInfo.xml and the files Windows and macOS
        // leave, like Thumbs.db or .DS_Store, are deleted, and the folder only when nothing else is left
        Permanently
    };
    using Trash = std::function<bool(const QString &path)>;

    explicit ComicsRemover(QModelIndexList &indexList, QList<QString> &paths, qulonglong parentId, FolderDeletion folderDeletion, QObject *parent = nullptr);
    // the system trash unless a test replaces it
    void setTrash(Trash trash) { this->trash = std::move(trash); }

signals:
    void remove(int);
    // positions in the lists given to the constructor
    void removeError(int position);
    // a comic file the trash refused, which is deleted for good as it always was
    void fileDeletedPermanently();
    void folderNotTrashed(int position);
    // with the names of what is left in the folder
    void folderNotEmptied(int position, const QStringList &leftovers);
    void finished();
    void removedItemsFromFolder(qulonglong);

public slots:
    void process();

private:
    enum class PermanentDeletion {
        Deleted,
        FilesLeft,
        Failed
    };

    void removeFile(int position, int row, const QString &path);
    void removeFolder(int position, int row, const QString &path);
    PermanentDeletion deletePermanently(const QString &path, const FolderComicContents &contents, QStringList *leftovers);

    QModelIndexList indexList;
    QList<QString> paths;
    qulonglong parentId;
    FolderDeletion folderDeletion;
    Trash trash = [](const QString &path) { return QFile::moveToTrash(path); };
};

class FoldersRemover : public QObject
{
    Q_OBJECT
public:
    explicit FoldersRemover(QModelIndexList &indexList, QList<QString> &paths, QObject *parent = nullptr);

signals:
    void remove(QModelIndex);
    void removeError();
    void finished();

public slots:
    void process();

private:
    QModelIndexList indexList;
    QList<QString> paths;
};

#endif // COMICS_REMOVER_H
