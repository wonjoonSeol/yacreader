#include "comic_db.h"
#include "comics_remover.h"
#include "delete_comics_question.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class DeleteComicsQuestionTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void enterAnswersNo_data();
    void enterAnswersNo();
    void asksNothingMoreWithoutFolderComics();
    void keepsFolderComicsUnlessTheBoxIsChecked();
    void waitsForTheBoxWithOnlyFolderComics();

private:
    QTemporaryDir root;
};

namespace {

ComicDB comicAt(const QString &path)
{
    ComicDB comic;
    comic.path = path;
    comic.info.numPages = 3;
    return comic;
}

int answerOfEnter(QMessageBox &question)
{
    QTimer::singleShot(0, &question, [&question] { QTest::keyClick(&question, Qt::Key_Return); });
    return question.exec();
}

}

void DeleteComicsQuestionTest::initTestCase()
{
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.filePath("Chapter")));
    QFile book(root.filePath("Book.cbz"));
    QVERIFY(book.open(QIODevice::WriteOnly));
}

void DeleteComicsQuestionTest::enterAnswersNo_data()
{
    QTest::addColumn<QStringList>("paths");

    QTest::newRow("comic files") << QStringList { "/Book.cbz" };
    QTest::newRow("comic files and folder comics") << QStringList { "/Book.cbz", "/Chapter" };
}

void DeleteComicsQuestionTest::enterAnswersNo()
{
    QFETCH(QStringList, paths);
    QList<ComicDB> comics;
    for (const auto &path : paths) {
        comics.append(comicAt(path));
    }

    DeleteComicsQuestion question(ComicsDeletion(comics, root.path()));

    QCOMPARE(question.defaultButton(), question.button(QMessageBox::No));
    QCOMPARE(answerOfEnter(question), int(QMessageBox::No));
}

void DeleteComicsQuestionTest::asksNothingMoreWithoutFolderComics()
{
    DeleteComicsQuestion question(ComicsDeletion({ comicAt("/Book.cbz") }, root.path()));

    QCOMPARE(question.checkBox(), nullptr);
    QVERIFY(question.button(QMessageBox::Yes)->isEnabled());
    QVERIFY(!question.deletesFolders());
}

void DeleteComicsQuestionTest::keepsFolderComicsUnlessTheBoxIsChecked()
{
    DeleteComicsQuestion question(ComicsDeletion({ comicAt("/Book.cbz"), comicAt("/Chapter") }, root.path()));

    QVERIFY(question.checkBox() != nullptr);
    QVERIFY(!question.checkBox()->isChecked());
    QVERIFY(question.button(QMessageBox::Yes)->isEnabled());
    QVERIFY(!question.deletesFolders());

    question.checkBox()->setChecked(true);

    QVERIFY(question.deletesFolders());
}

void DeleteComicsQuestionTest::waitsForTheBoxWithOnlyFolderComics()
{
    DeleteComicsQuestion question(ComicsDeletion({ comicAt("/Chapter") }, root.path()));
    auto *yes = question.button(QMessageBox::Yes);

    QVERIFY(!yes->isEnabled());
    question.checkBox()->setChecked(true);
    QVERIFY(yes->isEnabled());
    question.checkBox()->setChecked(false);
    QVERIFY(!yes->isEnabled());
}

QTEST_MAIN(DeleteComicsQuestionTest)

#include "main.moc"
