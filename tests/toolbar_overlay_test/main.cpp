#include "mouse_handler.h"
#include "toolbar_overlay.h"

#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QSignalSpy>
#include <QTest>
#include <QToolBar>
#include <QToolButton>

#include <memory>

using YACReader::mouseOverActivationHeight;
using YACReader::ToolBarOverlay;

// A plain main window laid out like the reader's in full screen: the toolbar docked but hidden, a page with the page
// number label in the top right corner, and a widget lower on the page. Each move goes to the innermost native window
// under the point, the way the platform delivers it.
class ToolBarOverlayTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase_data();
    void init();
    void cleanup();

    void showsOnlyInTheBandAtTheTop_data();
    void showsOnlyInTheBandAtTheTop();
    void staysOnTheToolBarAndHidesBelowIt();
    void measuresTheMouseFromTheTopOfTheWindow();
    void doesNotShowWhileItCannotReveal();
    void showsAfterADragToTheTopOnlyOnceReleased();
    void staysDockedWhenShownByTheWindow();
    void docksAndStopsWhenDeactivated();
    void hidesWhenTheWindowChanges_data();
    void hidesWhenTheWindowChanges();
    void staysWhenOneOfItsMenusOpens();
    void reportsTheHeightItCovers();

private:
    void moveTo(QPoint inWindow);
    QWindow *nativeWindowAt(QPoint inWindow, QPoint *inNativeWindow) const;
    void verifyDockedAndHidden() const;

    std::unique_ptr<QMainWindow> window;
    QToolBar *toolBar = nullptr;
    QToolButton *menuButton = nullptr;
    QWidget *page = nullptr;
    QLabel *pageNumber = nullptr;
    QWidget *lowerOnThePage = nullptr;
    ToolBarOverlay *overlay = nullptr;
    bool canReveal = true;
    QRect pageGeometry;
};

void ToolBarOverlayTest::initTestCase_data()
{
    QTest::addColumn<bool>("native");

    QTest::newRow("plain widgets") << false;
    // the go-to flow's window container does this on its first show
    QTest::newRow("native child windows") << true;
}

void ToolBarOverlayTest::init()
{
    QFETCH_GLOBAL(bool, native);

    window = std::make_unique<QMainWindow>();
    window->setGeometry(100, 100, 800, 600);

    toolBar = window->addToolBar(QStringLiteral("File"));
    toolBar->setMovable(false);
    menuButton = new QToolButton;
    menuButton->setText(QStringLiteral("Open"));
    // taller than the band at the top, so the band and the toolbar height are different limits
    menuButton->setFixedSize(60, 40);
    menuButton->setMenu(new QMenu(menuButton));
    menuButton->menu()->addAction(QStringLiteral("Open folder"));
    toolBar->addWidget(menuButton);
    toolBar->hide();

    page = new QWidget;
    window->setCentralWidget(page);
    pageNumber = new QLabel(QStringLiteral("12/24"), page);
    pageNumber->setGeometry(700, 0, 100, 20);
    lowerOnThePage = new QWidget(page);
    lowerOnThePage->setGeometry(0, 100, 800, 500);

    canReveal = true;
    overlay = new ToolBarOverlay(window.get(), toolBar, [this] { return canReveal; });
    overlay->setActive(true);

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));

    if (native) {
        lowerOnThePage->winId();
        QVERIFY(page->internalWinId());
        QVERIFY(pageNumber->internalWinId());
        QVERIFY(toolBar->internalWinId());
    }

    QVERIFY(!pageNumber->hasMouseTracking());
    QVERIFY(!menuButton->hasMouseTracking());
    pageGeometry = page->geometry();
    QCOMPARE(pageGeometry, QRect(0, 0, 800, 600));

    // Qt drops a move to where the mouse already is, so every test starts away from the points it moves to
    moveTo({ 450, 450 });
    QVERIFY(!overlay->isOverPage());
}

void ToolBarOverlayTest::cleanup()
{
    // the overlay goes first, so its application filter doesn't see the window's destruction
    delete overlay;
    overlay = nullptr;
    window.reset();
}

QWindow *ToolBarOverlayTest::nativeWindowAt(QPoint inWindow, QPoint *inNativeWindow) const
{
    QWidget *target = window->childAt(inWindow);
    if (target == nullptr)
        target = window.get();
    while (target->windowHandle() == nullptr)
        target = target->parentWidget();

    *inNativeWindow = target->mapFrom(window.get(), inWindow);
    return target->windowHandle();
}

void ToolBarOverlayTest::moveTo(QPoint inWindow)
{
    QPoint inNativeWindow;
    QWindow *nativeWindow = nativeWindowAt(inWindow, &inNativeWindow);
    QTest::mouseMove(nativeWindow, inNativeWindow);
}

void ToolBarOverlayTest::verifyDockedAndHidden() const
{
    QVERIFY(!overlay->isOverPage());
    QCOMPARE(window->toolBarArea(toolBar), Qt::TopToolBarArea);
    QVERIFY(toolBar->isHidden());
    QCOMPARE(page->geometry(), pageGeometry);
}

void ToolBarOverlayTest::showsOnlyInTheBandAtTheTop_data()
{
    QTest::addColumn<QPoint>("mouse");
    QTest::addColumn<bool>("shown");

    // not (0, 0): QTest moves to the middle of the window for a null point
    QTest::newRow("top left corner") << QPoint(1, 0) << true;
    QTest::newRow("top of the page") << QPoint(400, 5) << true;
    QTest::newRow("page number label in the top right corner") << QPoint(790, 5) << true;
    QTest::newRow("bottom of the band") << QPoint(400, mouseOverActivationHeight - 1) << true;
    QTest::newRow("just below the band") << QPoint(400, mouseOverActivationHeight) << false;
    QTest::newRow("below the band, where the toolbar would be") << QPoint(400, 25) << false;
    QTest::newRow("middle of the page") << QPoint(400, 300) << false;
}

void ToolBarOverlayTest::showsOnlyInTheBandAtTheTop()
{
    QFETCH(QPoint, mouse);
    QFETCH(bool, shown);

    moveTo(mouse);

    QCOMPARE(overlay->isOverPage(), shown);
    QCOMPARE(toolBar->isVisible(), shown);
    if (shown) {
        QCOMPARE(toolBar->geometry(), QRect(0, 0, window->width(), toolBar->sizeHint().height()));
        QVERIFY(toolBar->height() > 25);
    }
    // laid over the page, not docked above it
    QCOMPARE(page->geometry(), pageGeometry);
    QCOMPARE(toolBar->parentWidget(), window.get());
}

void ToolBarOverlayTest::staysOnTheToolBarAndHidesBelowIt()
{
    moveTo({ 400, 0 });
    QVERIFY(overlay->isOverPage());
    const int height = toolBar->height();

    moveTo(menuButton->mapTo(window.get(), menuButton->rect().center()));
    QVERIFY(overlay->isOverPage());
    moveTo({ 400, height - 1 });
    QVERIFY(overlay->isOverPage());
    QCOMPARE(page->geometry(), pageGeometry);

    moveTo({ 400, height });
    verifyDockedAndHidden();

    // and again, with the same toolbar
    moveTo({ 400, 0 });
    QVERIFY(overlay->isOverPage());
    QCOMPARE(toolBar->parentWidget(), window.get());
}

void ToolBarOverlayTest::measuresTheMouseFromTheTopOfTheWindow()
{
    // the move reaches the filter at the widget under the mouse, whose own coordinates are 100 px off
    const QPoint nearTheTopOfTheLowerWidget(10, 105);
    QCOMPARE(window->childAt(nearTheTopOfTheLowerWidget), lowerOnThePage);

    moveTo(nearTheTopOfTheLowerWidget);
    QVERIFY(!overlay->isOverPage());

    moveTo({ 10, 5 });
    QVERIFY(overlay->isOverPage());
    moveTo(nearTheTopOfTheLowerWidget);
    QVERIFY(!overlay->isOverPage());
}

void ToolBarOverlayTest::doesNotShowWhileItCannotReveal()
{
    canReveal = false;
    moveTo({ 400, 0 });
    QVERIFY(!overlay->isOverPage());

    canReveal = true;
    moveTo({ 400, 1 });
    QVERIFY(overlay->isOverPage());

    // only showing it is up to canReveal
    canReveal = false;
    moveTo({ 400, 20 });
    QVERIFY(overlay->isOverPage());
}

void ToolBarOverlayTest::showsAfterADragToTheTopOnlyOnceReleased()
{
    QPoint inNativeWindow;
    QWindow *nativeWindow = nativeWindowAt({ 400, 300 }, &inNativeWindow);
    QTest::mousePress(nativeWindow, Qt::LeftButton, { }, inNativeWindow);
    for (int y = 300; y >= 0; y -= 50) {
        QTest::mouseMove(nativeWindow, inNativeWindow + QPoint(0, y - 300));
        QVERIFY(!overlay->isOverPage());
    }
    QTest::mouseRelease(nativeWindow, Qt::LeftButton, { }, inNativeWindow + QPoint(0, -300));
    QVERIFY(!overlay->isOverPage());

    moveTo({ 400, 1 });
    QVERIFY(overlay->isOverPage());
}

void ToolBarOverlayTest::staysDockedWhenShownByTheWindow()
{
    moveTo({ 400, 0 });
    QVERIFY(overlay->isOverPage());

    // what the reader's toolbar toggle (H) does
    overlay->dock();
    toolBar->show();
    QCOMPARE(window->toolBarArea(toolBar), Qt::TopToolBarArea);
    QVERIFY(toolBar->isVisible());
    QTRY_COMPARE(page->y(), toolBar->height());

    moveTo({ 300, 1 });
    QVERIFY(!overlay->isOverPage());
    moveTo({ 400, 300 });
    QVERIFY(!overlay->isOverPage());
    QVERIFY(toolBar->isVisible());
}

void ToolBarOverlayTest::docksAndStopsWhenDeactivated()
{
    moveTo({ 400, 0 });
    QVERIFY(overlay->isOverPage());

    overlay->setActive(false);
    verifyDockedAndHidden();

    moveTo({ 400, 1 });
    verifyDockedAndHidden();

    overlay->setActive(true);
    moveTo({ 400, 0 });
    QVERIFY(overlay->isOverPage());
}

void ToolBarOverlayTest::hidesWhenTheWindowChanges_data()
{
    QTest::addColumn<int>("change");

    QTest::newRow("the mouse leaves the window") << int(QEvent::Leave);
    QTest::newRow("another window is activated") << int(QEvent::WindowDeactivate);
    QTest::newRow("the window is resized") << int(QEvent::Resize);
    QTest::newRow("the window manager takes it out of full screen") << int(QEvent::WindowStateChange);
}

void ToolBarOverlayTest::hidesWhenTheWindowChanges()
{
    QFETCH(int, change);

    moveTo({ 400, 0 });
    QVERIFY(overlay->isOverPage());

    if (change == QEvent::Resize) {
        window->resize(700, 600);
        QTRY_COMPARE(page->width(), 700);
        pageGeometry = page->geometry();
    } else if (change == QEvent::WindowStateChange) {
        QWindowStateChangeEvent event(Qt::WindowFullScreen);
        QApplication::sendEvent(window.get(), &event);
    } else {
        QEvent event(static_cast<QEvent::Type>(change));
        QApplication::sendEvent(window.get(), &event);
    }
    verifyDockedAndHidden();

    moveTo({ 300, 2 });
    QVERIFY(overlay->isOverPage());
    QCOMPARE(toolBar->width(), window->width());
}

void ToolBarOverlayTest::staysWhenOneOfItsMenusOpens()
{
    moveTo({ 400, 0 });
    QVERIFY(overlay->isOverPage());

    menuButton->menu()->popup(menuButton->mapToGlobal(QPoint(0, menuButton->height())));
    QVERIFY(QApplication::activePopupWidget() == menuButton->menu());

    // opening the menu makes the mouse leave the window
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(window.get(), &leave);
    QEvent deactivate(QEvent::WindowDeactivate);
    QApplication::sendEvent(window.get(), &deactivate);
    QVERIFY(overlay->isOverPage());
    QVERIFY(toolBar->isVisible());

    menuButton->menu()->close();
    QTRY_VERIFY(QApplication::activePopupWidget() == nullptr);
    QApplication::sendEvent(window.get(), &leave);
    verifyDockedAndHidden();
}

void ToolBarOverlayTest::reportsTheHeightItCovers()
{
    QSignalSpy covered(overlay, &ToolBarOverlay::coveredHeightChanged);

    moveTo({ 400, 0 });
    QCOMPARE(covered.count(), 1);
    QCOMPARE(covered.takeFirst().at(0).toInt(), toolBar->height());
    QVERIFY(toolBar->height() > 0);

    moveTo({ 400, 300 });
    QCOMPARE(covered.count(), 1);
    QCOMPARE(covered.takeFirst().at(0).toInt(), 0);

    // docking a docked toolbar again reports nothing
    overlay->dock();
    QCOMPARE(covered.count(), 0);

    moveTo({ 400, 0 });
    overlay->setActive(false);
    QCOMPARE(covered.count(), 2);
    QCOMPARE(covered.at(1).at(0).toInt(), 0);
}

QTEST_MAIN(ToolBarOverlayTest)

#include "main.moc"
