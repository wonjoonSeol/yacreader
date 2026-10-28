#include "library_entry.h"

#include "comic.h"
#include "yacreader_global.h"

#include <QCryptographicHash>
#include <QFile>

#include <algorithm>
#include <utility>

namespace {

QByteArray firstBytes(const QFileInfo &fileInfo)
{
    QFile file(fileInfo.absoluteFilePath());
    return file.open(QFile::ReadOnly) ? file.read(524288) : QByteArray();
}

qint64 totalSize(const QFileInfoList &files)
{
    qint64 size = 0;
    for (const QFileInfo &file : files) {
        size += file.size();
    }
    return size;
}

// hash Sha1 del primer 0.5MB + filesize
QString filePseudoHash(const QFileInfo &fileInfo)
{
    QCryptographicHash crypto(QCryptographicHash::Sha1);
    crypto.addData(firstBytes(fileInfo));
    return QString(crypto.result().toHex().constData()) + QString::number(fileInfo.size());
}

// Sha1 of the first 0.5MB of the first and last pages and of the name and size of every page, then, like a
// file hash, the size that ComicDB::getFileSize() reads back, here the size of all the pages.
// The pages go in the UTF-16 order of their NFC names and not in reading order, which depends on the locale
// and on how Qt was built, so that every app that shares a library computes the same hash. NFC because
// macOS keeps names decomposed.
QString folderPseudoHash(const QFileInfoList &pages)
{
    QList<std::pair<QString, QFileInfo>> namedPages;
    namedPages.reserve(pages.size());
    for (const QFileInfo &page : pages) {
        namedPages.append({ page.fileName().normalized(QString::NormalizationForm_C), page });
    }
    std::sort(namedPages.begin(), namedPages.end(), [](const auto &left, const auto &right) {
        return left.first != right.first ? left.first < right.first : left.second.fileName() < right.second.fileName();
    });

    QCryptographicHash crypto(QCryptographicHash::Sha1);
    crypto.addData(firstBytes(namedPages.constFirst().second));
    if (namedPages.size() > 1) {
        crypto.addData(firstBytes(namedPages.constLast().second));
    }
    for (const auto &[name, page] : std::as_const(namedPages)) {
        crypto.addData(name.toUtf8() + '/' + QByteArray::number(page.size()) + '\n');
    }
    return QString(crypto.result().toHex().constData()) + QString::number(totalSize(pages));
}

}

namespace YACReader {

QList<LibraryEntry> LibraryEntry::entriesIn(const QFileInfoList &listing)
{
    QList<LibraryEntry> entries;
    for (const QFileInfo &info : listing) {
        if (!info.isDir()) {
            if (Comic::fileIsComic(info.filePath())) {
                entries.append(LibraryEntry { info });
            }
            continue;
        }
        // the data folder of a library, this one or one in it, is the only folder left out by name
        if (info.absoluteFilePath() == LibraryPaths::libraryDataPath(info.absolutePath())) {
            continue;
        }

        LibraryEntry entry { info };
        entry.listing = FolderComic::listing(info.absoluteFilePath());
        entry.folderComic = FolderComic::comicContents(info.absoluteFilePath(), entry.listing);
        if (entry.folderComic) {
            entry.listing.clear();
        }
        entries.append(entry);
    }
    return entries;
}

qint64 LibraryEntry::size() const
{
    return folderComic ? totalSize(folderComic->pages) : info.size();
}

// a folder's own date changes when other files such as ComicInfo.xml come and go, and not when a page is edited
QDateTime LibraryEntry::lastModified() const
{
    if (!folderComic) {
        return info.lastModified();
    }
    QDateTime newest;
    for (const QFileInfo &page : folderComic->pages) {
        newest = std::max(newest, page.lastModified());
    }
    return newest;
}

QString LibraryEntry::pseudoHash() const
{
    return folderComic ? folderPseudoHash(folderComic->pages) : filePseudoHash(info);
}

}
