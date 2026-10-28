#include "toolbar_over_page.h"

#include <QObject>
#include <QTest>

using YACReader::mouseOverActivationHeight;
using YACReader::shouldShowToolBarOverPage;
using YACReader::ToolBarOverPageState;

class ToolBarOverPageTest : public QObject
{
    Q_OBJECT

private slots:
    void showsTheToolBarUntilTheMouseMovesBelowIt_data();
    void showsTheToolBarUntilTheMouseMovesBelowIt();
};

namespace {

// taller than the band that shows the toolbar, so the two limits differ
constexpr int toolBarHeight = 40;
static_assert(toolBarHeight > mouseOverActivationHeight);

}

void ToolBarOverPageTest::showsTheToolBarUntilTheMouseMovesBelowIt_data()
{
    QTest::addColumn<bool>("shown");
    QTest::addColumn<int>("mouseY");
    QTest::addColumn<bool>("canReveal");
    QTest::addColumn<bool>("mouseButtonPressed");
    QTest::addColumn<bool>("expected");

    QTest::newRow("hidden, mouse at the top of the screen") << false << 0 << true << false << true;
    QTest::newRow("hidden, mouse at the bottom of the band") << false << mouseOverActivationHeight - 1 << true << false << true;
    QTest::newRow("hidden, mouse below the band") << false << mouseOverActivationHeight << true << false << false;
    QTest::newRow("hidden, mouse below the band but where the toolbar would be") << false << 30 << true << false << false;
    QTest::newRow("hidden, mouse at the top while it can't reveal") << false << 0 << false << false << false;
    QTest::newRow("hidden, mouse dragged to the top") << false << 0 << true << true << false;
    QTest::newRow("shown, mouse below the band but on the toolbar") << true << 30 << true << false << true;
    QTest::newRow("shown, mouse at the bottom of the toolbar") << true << toolBarHeight - 1 << true << false << true;
    QTest::newRow("shown, mouse just below the toolbar") << true << toolBarHeight << true << false << false;
    QTest::newRow("shown, mouse on the page") << true << 500 << true << false << false;
    QTest::newRow("shown, mouse on the toolbar while it can't reveal") << true << 30 << false << false << true;
    QTest::newRow("shown, mouse dragged on the toolbar") << true << 30 << true << true << true;
}

void ToolBarOverPageTest::showsTheToolBarUntilTheMouseMovesBelowIt()
{
    QFETCH(bool, shown);
    QFETCH(int, mouseY);
    QFETCH(bool, canReveal);
    QFETCH(bool, mouseButtonPressed);
    QFETCH(bool, expected);

    const ToolBarOverPageState state { .shown = shown,
                                       .mouseY = mouseY,
                                       .toolBarHeight = toolBarHeight,
                                       .canReveal = canReveal,
                                       .mouseButtonPressed = mouseButtonPressed };

    QCOMPARE(shouldShowToolBarOverPage(state), expected);
}

QTEST_GUILESS_MAIN(ToolBarOverPageTest)

#include "main.moc"
