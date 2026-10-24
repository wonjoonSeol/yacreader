#include "toolbar_over_page.h"

#include <QObject>
#include <QTest>

using YACReader::shouldShowToolBarOverPage;
using YACReader::toolBarOverPageActivationHeight;
using YACReader::ToolBarOverPageState;

class ToolBarOverPageTest : public QObject
{
    Q_OBJECT

private slots:
    void showsTheToolBarUntilTheMouseLeavesIt_data();
    void showsTheToolBarUntilTheMouseLeavesIt();
};

namespace {

// taller than the band that shows the toolbar, so the two limits differ
constexpr int toolBarHeight = 40;
static_assert(toolBarHeight > toolBarOverPageActivationHeight);

}

void ToolBarOverPageTest::showsTheToolBarUntilTheMouseLeavesIt_data()
{
    QTest::addColumn<bool>("shown");
    QTest::addColumn<int>("mouseY");
    QTest::addColumn<bool>("magnifyingGlassShown");
    QTest::addColumn<bool>("mouseButtonPressed");
    QTest::addColumn<bool>("expected");

    QTest::newRow("hidden, mouse at the top of the screen") << false << 0 << false << false << true;
    QTest::newRow("hidden, mouse at the bottom of the band") << false << toolBarOverPageActivationHeight - 1 << false << false << true;
    QTest::newRow("hidden, mouse below the band") << false << toolBarOverPageActivationHeight << false << false << false;
    QTest::newRow("hidden, mouse below the band but where the toolbar would be") << false << 30 << false << false << false;
    QTest::newRow("hidden, mouse at the top with the magnifying glass") << false << 0 << true << false << false;
    QTest::newRow("hidden, mouse dragged to the top") << false << 0 << false << true << false;
    QTest::newRow("shown, mouse below the band but on the toolbar") << true << 30 << false << false << true;
    QTest::newRow("shown, mouse at the bottom of the toolbar") << true << toolBarHeight - 1 << false << false << true;
    QTest::newRow("shown, mouse just below the toolbar") << true << toolBarHeight << false << false << false;
    QTest::newRow("shown, mouse on the page") << true << 500 << false << false << false;
    QTest::newRow("shown, mouse on the toolbar with the magnifying glass") << true << 30 << true << false << true;
    QTest::newRow("shown, mouse dragged on the toolbar") << true << 30 << false << true << true;
}

void ToolBarOverPageTest::showsTheToolBarUntilTheMouseLeavesIt()
{
    QFETCH(bool, shown);
    QFETCH(int, mouseY);
    QFETCH(bool, magnifyingGlassShown);
    QFETCH(bool, mouseButtonPressed);
    QFETCH(bool, expected);

    const ToolBarOverPageState state { .shown = shown,
                                       .mouseY = mouseY,
                                       .toolBarHeight = toolBarHeight,
                                       .magnifyingGlassShown = magnifyingGlassShown,
                                       .mouseButtonPressed = mouseButtonPressed };

    QCOMPARE(shouldShowToolBarOverPage(state), expected);
}

QTEST_GUILESS_MAIN(ToolBarOverPageTest)

#include "main.moc"
