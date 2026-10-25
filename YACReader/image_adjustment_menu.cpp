#include "image_adjustment_menu.h"

#include "configuration.h"
#include "image_adjustment.h"
#include "yacreader_spin_slider_widget.h"

#include <QActionGroup>
#include <QCoreApplication>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QWidgetAction>

namespace {

// The menu uses the labels of Options > Image adjustment, so both read the same in every language
QString optionsDialogText(const char *sourceText)
{
    return QCoreApplication::translate("OptionsDialog", sourceText);
}

void showValue(YACReaderSpinSliderWidget *slider, int value)
{
    // every setValue() call connects one more slot inside the slider
    if (slider->getValue() == value)
        return;

    const QSignalBlocker blocker(slider);
    slider->setValue(value);
}

}

ImageAdjustmentMenu::ImageAdjustmentMenu(QWidget *parent)
    : QMenu(optionsDialogText(QT_TRANSLATE_NOOP("OptionsDialog", "Image adjustment")), parent)
{
    addFilterSliders();
    addSeparator();
    addFitOptions();
    addScalingMethods();

    connect(this, &QMenu::aboutToShow, this, &ImageAdjustmentMenu::showStoredOptions);
}

void ImageAdjustmentMenu::addFilterSliders()
{
    auto *filters = new QWidget();
    auto *layout = new QGridLayout(filters);

    const auto addSlider = [layout](int row, const char *label, ImageFilterRange range) {
        auto *slider = new YACReaderSpinSliderWidget(nullptr, true);
        slider->layout()->setContentsMargins(0, 0, 0, 0);
        slider->setRange(range.minimum, range.maximum);
        slider->setTracking(false);
        layout->addWidget(new QLabel(optionsDialogText(label)), row, 0);
        layout->addWidget(slider, row, 1);
        return slider;
    };
    brightnessSlider = addSlider(0, QT_TRANSLATE_NOOP("OptionsDialog", "Brightness"), brightnessRange);
    contrastSlider = addSlider(1, QT_TRANSLATE_NOOP("OptionsDialog", "Contrast"), contrastRange);
    gammaSlider = addSlider(2, QT_TRANSLATE_NOOP("OptionsDialog", "Gamma"), gammaRange);

    auto *resetButton = new QPushButton(optionsDialogText(QT_TRANSLATE_NOOP("OptionsDialog", "Reset")));
    layout->addWidget(resetButton, 3, 0);
    layout->setColumnStretch(1, 1);

    for (auto *slider : { brightnessSlider, contrastSlider, gammaSlider }) {
        connect(slider, &YACReaderSpinSliderWidget::valueChanged, this, [this] {
            emit filtersChanged(brightnessSlider->getValue(), contrastSlider->getValue(), gammaSlider->getValue());
        });
    }
    connect(resetButton, &QAbstractButton::clicked, this, [this] {
        emit resetFiltersRequested();
        showStoredFilters();
    });

    auto *filtersAction = new QWidgetAction(this);
    filtersAction->setDefaultWidget(filters);
    addAction(filtersAction);
}

void ImageAdjustmentMenu::addFitOptions()
{
    enlargeImagesAction = addAction(optionsDialogText(QT_TRANSLATE_NOOP("OptionsDialog", "Enlarge images to fit width/height")));
    enlargeImagesAction->setCheckable(true);
    connect(enlargeImagesAction, &QAction::triggered, this, [this](bool checked) {
        Configuration::getConfiguration().setEnlargeImages(checked);
        emit imageOptionsChanged();
    });

    auto *continuousScrollMenu = addMenu(optionsDialogText(QT_TRANSLATE_NOOP("OptionsDialog", "Continuous scroll")));
    auto *continuousScrollFit = new QActionGroup(this);
    continuousScrollFitToWidthAction = continuousScrollMenu->addAction(optionsDialogText(QT_TRANSLATE_NOOP("OptionsDialog", "Fit pages to width")));
    continuousScrollFitToHeightAction = continuousScrollMenu->addAction(optionsDialogText(QT_TRANSLATE_NOOP("OptionsDialog", "Fit pages to height")));
    for (auto *action : { continuousScrollFitToWidthAction, continuousScrollFitToHeightAction }) {
        action->setCheckable(true);
        continuousScrollFit->addAction(action);
    }
    connect(continuousScrollFit, &QActionGroup::triggered, this, [this](QAction *action) {
        Configuration::getConfiguration().setContinuousScrollFitToHeight(action == continuousScrollFitToHeightAction);
        emit imageOptionsChanged();
    });
}

void ImageAdjustmentMenu::addScalingMethods()
{
    auto *scalingMenu = addMenu(optionsDialogText(QT_TRANSLATE_NOOP("OptionsDialog", "Scaling method")));
    auto *scalingMethods = new QActionGroup(this);
    for (const auto &choice : scalingMethodChoices()) {
        auto *action = scalingMenu->addAction(choice.text());
        action->setCheckable(true);
        scalingMethods->addAction(action);
        connect(action, &QAction::triggered, this, [this, method = choice.method] {
            Configuration::getConfiguration().setScalingMethod(method);
            emit imageOptionsChanged();
        });
        scalingMethodActions << action;
    }
}

void ImageAdjustmentMenu::showStoredOptions()
{
    auto &configuration = Configuration::getConfiguration();

    showStoredFilters();
    enlargeImagesAction->setChecked(configuration.getEnlargeImages());
    if (configuration.getContinuousScrollFitToHeight())
        continuousScrollFitToHeightAction->setChecked(true);
    else
        continuousScrollFitToWidthAction->setChecked(true);

    const int scalingMethodIndex = scalingMethodChoiceIndex(scalingMethodChoices(), configuration.getScalingMethod());
    for (int i = 0; i < scalingMethodActions.size(); i++)
        scalingMethodActions.at(i)->setChecked(i == scalingMethodIndex);
}

void ImageAdjustmentMenu::showStoredFilters()
{
    auto &configuration = Configuration::getConfiguration();
    showValue(brightnessSlider, configuration.getBrightness());
    showValue(contrastSlider, configuration.getContrast());
    showValue(gammaSlider, configuration.getGamma());
}
