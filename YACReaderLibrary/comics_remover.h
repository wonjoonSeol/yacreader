#ifndef COMICS_REMOVER_H
#define COMICS_REMOVER_H

#include <QModelIndex>
#include <QStringList>
#include <QThread>

// What deleting some comics removes from disk: comic files always, folder comics only when
// their image folders are deleted too. Positions refer to the list of comic paths.
struct ComicsDeletion {
    explicit ComicsDeletion(const QStringList &comicPaths);
    QList<int> deleted(bool deleteFolders) const;

    QList<int> files;
    QList<int> folders;
    int folderImages = 0;
};

class ComicsRemover : public QObject
{
    Q_OBJECT
public:
    explicit ComicsRemover(QModelIndexList &indexList, QList<QString> &paths, qulonglong parentId, QObject *parent = nullptr);
    // folders skip the trash, otherwise a folder the trash refuses is only reported with folderNotTrashed
    void setDeleteFoldersPermanently(bool permanently) { deleteFoldersPermanently = permanently; }

signals:
    void remove(int);
    void removeError();
    void folderNotTrashed(const QString &path);
    void finished();
    void removedItemsFromFolder(qulonglong);

public slots:
    void process();

private:
    void removeFolder(int row, const QString &path);

    QModelIndexList indexList;
    QList<QString> paths;
    qulonglong parentId;
    bool deleteFoldersPermanently = false;
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
