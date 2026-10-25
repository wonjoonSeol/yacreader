#include "shortcuts_manager.h"
#include "yacreader_global.h"

#include <QAction>
#include <QFile>
#include <QGuiApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTest>

// Covers the shortcuts added after users could already customize theirs.
class ShortcutDefaultsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void aNewDefaultGivesWayToAKeyTheUserSaved();
};

namespace {
QString settingsFilePath()
{
    return YACReader::getSettingsPath() + "/YACReader.ini";
}

void saveShortcut(const QString &name, const QString &keys)
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    settings.beginGroup("shortcuts");
    settings.setValue(name, keys);
}

void loadShortcut(QAction &action, const QString &name)
{
    action.setData(name);
    action.setShortcut(ShortcutsManager::getShortcutsManager().getShortcut(name));
}
}

void ShortcutDefaultsTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QFile::remove(settingsFilePath());
}

void ShortcutDefaultsTest::cleanupTestCase()
{
    QFile::remove(settingsFilePath());
}

void ShortcutDefaultsTest::aNewDefaultGivesWayToAKeyTheUserSaved()
{
    // the user gave 1, the default of Rate 1 out of 5, to Go 5 pages to the left
    saveShortcut(GO_5_PAGES_TO_THE_LEFT_ACTION_Y, "1");

    QAction goFivePagesLeft;
    loadShortcut(goFivePagesLeft, GO_5_PAGES_TO_THE_LEFT_ACTION_Y);
    QAction rateOne;
    loadShortcut(rateOne, RATE_1_ACTION_Y);
    QAction rateTwo;
    loadShortcut(rateTwo, RATE_2_ACTION_Y);
    QCOMPARE(goFivePagesLeft.shortcut(), QKeySequence("1"));
    QCOMPARE(rateOne.shortcut(), QKeySequence("1"));

    auto &manager = ShortcutsManager::getShortcutsManager();
    manager.registerActions({ &goFivePagesLeft, &rateOne, &rateTwo });
    manager.clearConflictingDefaults({ &goFivePagesLeft, &rateOne, &rateTwo });

    QCOMPARE(goFivePagesLeft.shortcut(), QKeySequence("1"));
    QVERIFY(rateOne.shortcut().isEmpty());
    QCOMPARE(rateTwo.shortcut(), QKeySequence("2"));
}

int main(int argc, char *argv[])
{
    // QAction shortcuts need a QGuiApplication; ctest runs without a display
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    ShortcutDefaultsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "main.moc"
