#include "delete_comics_question.h"

#include "comics_remover.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QPushButton>

DeleteComicsQuestion::DeleteComicsQuestion(const ComicsDeletion &deletion, QWidget *parent)
    : QMessageBox(QMessageBox::Question,
                  QCoreApplication::translate("LibraryWindow", "Delete comics"),
                  QCoreApplication::translate("LibraryWindow", "All the selected comics will be deleted from your disk. Are you sure?"),
                  QMessageBox::Yes | QMessageBox::No,
                  parent)
{
    setDefaultButton(QMessageBox::No);
    if (deletion.folders.isEmpty()) {
        return;
    }

    setInformativeText(QCoreApplication::translate("LibraryWindow", "Folder comics stay on your disk and in the library unless you also delete their image folders."));
    const auto folders = QCoreApplication::translate("LibraryWindow", "%n folder(s)", nullptr, deletion.folders.size());
    const auto images = QCoreApplication::translate("LibraryWindow", "%n image(s)", nullptr, deletion.folderImages);
    deleteFoldersBox = new QCheckBox(QCoreApplication::translate("LibraryWindow", "Also delete the image folders (%1, %2)").arg(folders, images));
    setCheckBox(deleteFoldersBox);

    if (deletion.files.isEmpty()) {
        auto *yes = button(QMessageBox::Yes);
        yes->setEnabled(false);
        connect(deleteFoldersBox, &QCheckBox::toggled, yes, &QWidget::setEnabled);
    }
}

bool DeleteComicsQuestion::deletesFolders() const
{
    return deleteFoldersBox != nullptr && deleteFoldersBox->isChecked();
}
