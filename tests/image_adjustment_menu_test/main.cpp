#include "appearance_configuration.h"
#include "configuration.h"
#include "image_adjustment.h"
#include "image_adjustment_menu.h"
#include "image_filter_slider.h"
#include "options_dialog.h"
#include "theme_manager.h"
#include "theme_repository.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

// A comic's -1 means it has no level of its own
static_assert(brightnessSpec.levelOrDefault(-1) == 0);
static_assert(contrastSpec.levelOrDefault(-1) == 100);
static_assert(contrastSpec.levelOrDefault(40) == 40);
static_assert(contrastSpec.toComicLevel(100, -1) == -1);
static_assert(contrastSpec.toComicLevel(100, 120) == 100);
static_assert(contrastSpec.toComicLevel(40, -1) == 40);

namespace {

template<typename T>
T *childWithText(QObject *parent, const QString &text)
{
    for (auto *child : parent->findChildren<T *>()) {
        if (child->text() == text)
            return child;
    }
    return nullptr;
}

QString scalingMethodLabel(ScaleMethod method)
{
    for (const auto &choice : scalingMethodChoices()) {
        if (choice.method == method)
            return choice.text();
    }
    return { };
}

}

// Options > Image adjustment and the image adjustment menu, both on one Configuration, as in the reader
class ImageAdjustmentMenuTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void menuChangesShowInOptions();
    void optionsChangesShowInMenu();
    void filterLevelIsStoredOnceItStopsChanging();
    void resetStoresTheDefaultsAtOnce();
    void choosingTheCheckedEntryStoresNothing();
    void tabReachesEachSliderAndResetInTheMenu();

private:
    QAction *menuAction(const QString &text) const { return childWithText<QAction>(menu.get(), text); }
    QAction *scalingMethodAction(ScaleMethod method) const { return menuAction(scalingMethodLabel(method)); }
    QComboBox *scalingMethodCombo() const;
    QSpinBox *filterSpinBox(QWidget *parent, const QString &label) const;

    QTemporaryDir dir;
    std::unique_ptr<QSettings> settings;
    std::unique_ptr<OptionsDialog> dialog;
    std::unique_ptr<ImageAdjustmentMenu> menu;
};

void ImageAdjustmentMenuTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(dir.isValid());
    const QString iniPath = dir.filePath("YACReader.ini");

    ThemeManager::instance().initialize(new AppearanceConfiguration(iniPath, this),
                                        new ThemeRepository(":/themes", dir.filePath("themes"), "YACReader"));

    settings = std::make_unique<QSettings>(iniPath, QSettings::IniFormat);
    Configuration::getConfiguration().load(settings.get());

    dialog = std::make_unique<OptionsDialog>();
    dialog->restoreOptions(settings.get());
    menu = std::make_unique<ImageAdjustmentMenu>();
}

void ImageAdjustmentMenuTest::init()
{
    auto &configuration = Configuration::getConfiguration();
    configuration.setFilterLevels(brightnessSpec.defaultValue, contrastSpec.defaultValue, gammaSpec.defaultValue);
    configuration.setEnlargeImages(true);
    configuration.setContinuousScrollFitToHeight(false);
    configuration.setScalingMethod(ScaleMethod::Lanczos);
}

void ImageAdjustmentMenuTest::cleanupTestCase()
{
    menu.reset();
    dialog.reset();
}

QComboBox *ImageAdjustmentMenuTest::scalingMethodCombo() const
{
    for (auto *combo : dialog->findChildren<QComboBox *>()) {
        if (combo->findData(static_cast<int>(ScaleMethod::Lanczos2)) >= 0)
            return combo;
    }
    return nullptr;
}

QSpinBox *ImageAdjustmentMenuTest::filterSpinBox(QWidget *parent, const QString &label) const
{
    for (auto *slider : parent->findChildren<ImageFilterSlider *>()) {
        if (slider->findChild<QLabel *>()->text() == label)
            return slider->findChild<QSpinBox *>();
    }
    return nullptr;
}

void ImageAdjustmentMenuTest::menuChangesShowInOptions()
{
    auto &configuration = Configuration::getConfiguration();

    menuAction("Fit pages to height")->trigger();
    QVERIFY(configuration.getContinuousScrollFitToHeight());
    QVERIFY(childWithText<QRadioButton>(dialog.get(), "Fit pages to height")->isChecked());

    menuAction("Enlarge images to fit width/height")->trigger();
    QVERIFY(!configuration.getEnlargeImages());
    QVERIFY(!childWithText<QCheckBox>(dialog.get(), "Enlarge images to fit width/height")->isChecked());

    // stored as 4, listed fourth
    scalingMethodAction(ScaleMethod::Lanczos3)->trigger();
    QCOMPARE(configuration.getScalingMethod(), ScaleMethod::Lanczos3);
    QCOMPARE(selectedScalingMethod(scalingMethodCombo()), ScaleMethod::Lanczos3);

    filterSpinBox(menu.get(), "Brightness")->setValue(15);
    QTRY_COMPARE(configuration.getFilterLevel(brightnessSpec), 15);
    QCOMPARE(filterSpinBox(dialog.get(), "Brightness")->value(), 15);
}

void ImageAdjustmentMenuTest::optionsChangesShowInMenu()
{
    childWithText<QRadioButton>(dialog.get(), "Fit pages to height")->click();
    QVERIFY(menuAction("Fit pages to height")->isChecked());
    QVERIFY(!menuAction("Fit pages to width")->isChecked());

    childWithText<QCheckBox>(dialog.get(), "Enlarge images to fit width/height")->click();
    QVERIFY(!menuAction("Enlarge images to fit width/height")->isChecked());

    // stored as 5, listed fifth
    selectScalingMethod(scalingMethodCombo(), ScaleMethod::Lanczos2);
    QVERIFY(scalingMethodAction(ScaleMethod::Lanczos2)->isChecked());
    QVERIFY(!scalingMethodAction(ScaleMethod::Lanczos)->isChecked());

    filterSpinBox(dialog.get(), "Contrast")->setValue(120);
    QTRY_COMPARE(filterSpinBox(menu.get(), "Contrast")->value(), 120);
    QCOMPARE(filterSpinBox(menu.get(), "Brightness")->value(), brightnessSpec.defaultValue);
}

// Spin box steps closer together than the delay, as when a key repeats or the wheel turns
void ImageAdjustmentMenuTest::filterLevelIsStoredOnceItStopsChanging()
{
    QSignalSpy filtersChanged(&Configuration::getConfiguration(), &Configuration::filtersChanged);
    auto *gamma = filterSpinBox(menu.get(), "Gamma");
    const auto stepInterval = ImageFilterSlider::storeDelay / 3;

    for (int step = 0; step < 3; step++) {
        gamma->stepUp();
        QTest::qWait(stepInterval);
    }
    QCOMPARE(filtersChanged.count(), 0);
    QCOMPARE(Configuration::getConfiguration().getFilterLevel(gammaSpec), gammaSpec.defaultValue);

    QTRY_COMPARE(filtersChanged.count(), 1);
    QTest::qWait(ImageFilterSlider::storeDelay * 2);
    QCOMPARE(filtersChanged.count(), 1);
    const QList<QVariant> expected { brightnessSpec.defaultValue, contrastSpec.defaultValue, gammaSpec.defaultValue + 3 };
    QCOMPARE(filtersChanged.at(0), expected);
}

void ImageAdjustmentMenuTest::resetStoresTheDefaultsAtOnce()
{
    Configuration::getConfiguration().setFilterLevels(10, 120, 80);
    QCOMPARE(filterSpinBox(menu.get(), "Gamma")->value(), 80);
    QSignalSpy filtersChanged(&Configuration::getConfiguration(), &Configuration::filtersChanged);

    childWithText<QPushButton>(menu.get(), "Reset")->click();

    QCOMPARE(filtersChanged.count(), 1);
    const QList<QVariant> defaults { brightnessSpec.defaultValue, contrastSpec.defaultValue, gammaSpec.defaultValue };
    QCOMPARE(filtersChanged.at(0), defaults);
    for (auto *view : { static_cast<QWidget *>(dialog.get()), static_cast<QWidget *>(menu.get()) }) {
        QCOMPARE(filterSpinBox(view, "Brightness")->value(), brightnessSpec.defaultValue);
        QCOMPARE(filterSpinBox(view, "Contrast")->value(), contrastSpec.defaultValue);
        QCOMPARE(filterSpinBox(view, "Gamma")->value(), gammaSpec.defaultValue);
    }
}

void ImageAdjustmentMenuTest::choosingTheCheckedEntryStoresNothing()
{
    QSignalSpy imageOptionsChanged(&Configuration::getConfiguration(), &Configuration::imageOptionsChanged);

    menuAction("Fit pages to width")->trigger();
    scalingMethodAction(ScaleMethod::Lanczos)->trigger();
    QCOMPARE(imageOptionsChanged.count(), 0);

    menuAction("Fit pages to height")->trigger();
    QCOMPARE(imageOptionsChanged.count(), 1);
}

// QMenu moves the keyboard focus from entry to entry, so each slider needs an entry of its own
void ImageAdjustmentMenuTest::tabReachesEachSliderAndResetInTheMenu()
{
    menu->popup(QPoint(0, 0));
    QVERIFY(QTest::qWaitForWindowExposed(menu.get()));

    QTest::keyClick(menu.get(), Qt::Key_Down);
    QCOMPARE(menu->focusWidget(), filterSpinBox(menu.get(), "Brightness"));

    QTest::keyClick(menu->focusWidget(), Qt::Key_Tab);
    QCOMPARE(menu->focusWidget(), filterSpinBox(menu.get(), "Contrast"));

    QTest::keyClick(menu->focusWidget(), Qt::Key_Tab);
    QCOMPARE(menu->focusWidget(), filterSpinBox(menu.get(), "Gamma"));

    QTest::keyClick(menu->focusWidget(), Qt::Key_Tab);
    QCOMPARE(menu->focusWidget(), childWithText<QPushButton>(menu.get(), "Reset"));

    menu->hide();
}

QTEST_MAIN(ImageAdjustmentMenuTest)

#include "main.moc"
