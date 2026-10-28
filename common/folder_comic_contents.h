#ifndef FOLDER_COMIC_CONTENTS_H
#define FOLDER_COMIC_CONTENTS_H

#include <QFileInfo>
#include <QString>

// what the library reads from a folder comic, both from one listing of the folder
struct FolderComicContents {
    QFileInfoList pages; // in reading order
    QString comicInfoPath;
};

#endif // FOLDER_COMIC_CONTENTS_H
