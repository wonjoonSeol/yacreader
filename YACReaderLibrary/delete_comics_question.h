#ifndef DELETE_COMICS_QUESTION_H
#define DELETE_COMICS_QUESTION_H

#include <QMessageBox>

class QCheckBox;
struct ComicsDeletion;

// The question asked before deleting comics from disk. It defaults to No, and folder comics are
// only deleted with their image folders, so with only folder comics selected Yes waits for the box.
class DeleteComicsQuestion : public QMessageBox
{
public:
    DeleteComicsQuestion(const ComicsDeletion &deletion, QWidget *parent = nullptr);
    bool deletesFolders() const;

private:
    QCheckBox *deleteFoldersBox = nullptr;
};

#endif // DELETE_COMICS_QUESTION_H
