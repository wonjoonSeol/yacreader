#ifndef LIBRARY_ENTRY_H
#define LIBRARY_ENTRY_H

#include "folder_comic_contents.h"

#include <QDateTime>
#include <QFileInfo>
#include <QList>
#include <QString>

#include <optional>

namespace YACReader {

// An entry of a library folder: a folder that holds comics, or a comic, which is a file or a folder of images.
// The scan of a folder lists each folder in it once, to tell which of the two it is, and keeps that listing.
struct LibraryEntry {
    // the comics and the folders in a folder, from its FolderComic::listing()
    static QList<LibraryEntry> entriesIn(const QFileInfoList &listing);

    QFileInfo info;
    // what one listing found in a folder comic, for its size, hash, cover and date
    std::optional<FolderComicContents> folderComic;
    // what a folder that holds comics has inside, for the scan of that folder
    QFileInfoList listing;

    bool holdsComics() const { return info.isDir() && !folderComic; }
    // for a folder comic these come from its pages, which take the place of the archive file
    qint64 size() const;
    QDateTime lastModified() const;
    QString pseudoHash() const;
};

}

#endif // LIBRARY_ENTRY_H
