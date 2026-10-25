#ifndef INITIALCOMICINFOEXTRACTOR_H
#define INITIALCOMICINFOEXTRACTOR_H

#include "folder_comic_contents.h"

#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QProcess>
#include <QString>

#include <optional>

namespace YACReader {
class InitialComicInfoExtractor : public QObject
{
    Q_OBJECT

public:
    InitialComicInfoExtractor(QString fileSource, QString target = "", int coverPage = 1, bool getXMLMetadata = false);
    // reuses what the caller already listed of a folder comic instead of listing it again
    void setFolderComic(const FolderComicContents &contents) { _folderComic = contents; }

private:
    std::optional<FolderComicContents> _folderComic;
    QString _fileSource;
    QString _target;
    QString _currentName;
    int _numPages;
    QPair<int, int> _coverSize;
    QImage _cover;
    bool _coverExtracted;
    int _coverPage;
    bool _fileSupported;
    int getXMLMetadata;
    static bool crash;
    QByteArray _xmlInfoData;
    void extractFromFolder();
    void saveCover(const QString &path, const QImage &cover);

public slots:
    void extract();
    int getNumPages() { return _numPages; }
    QPixmap getCover() { return QPixmap::fromImage(_cover); }
    QImage getCoverImage() const { return _cover; }
    bool hasValidCover() const { return _coverExtracted; }
    // False when the file is not a comic YACReader can show, e.g. an EPUB book made of
    // text. Such a file has to be ignored, not stored as a comic without pages.
    bool isFileSupported() const { return _fileSupported; }
    QPair<int, int> getOriginalCoverSize() { return _coverSize; }
    QByteArray getXMLInfoRawData();
signals:
    void openingError(QProcess::ProcessError error);
};
}

#endif // INITIALCOMICINFOEXTRACTOR_H
