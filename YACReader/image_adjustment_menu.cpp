#include "image_adjustment_menu.h"

#include "configuration.h"
#include "image_adjustment.h"
#include "image_filter_slider.h"

#include <QActionGroup>
#include <QCoreApplication>
#include <QStyle>
#include <QWidgetAction>

// The labels are those of Options, in its translation context, so both read the same in every language

ImageAdjustmentMenu::ImageAdjustmentMenu(QWidget *parent)
    : QMenu(QCoreApplication::translate("OptionsDialog", "Image adjustment"), parent)
{
    addFilterSliders();
    addSeparator();
    addFitOptions();
    addScalingMethods();

    connect(&Configuration::getConfiguration(), &Configuration::imageOptionsChanged, this, &ImageAdjustmentMenu::showStoredOptions);
    showStoredOptions();
}

void ImageAdjustmentMenu::addFilterSliders()
{
    // the widgets would otherwise touch the edges of the menu and each other
    const int horizontalMargin = style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
    const int verticalMargin = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2;

    for (auto *widget : createImageFilterWidgets()) {
        widget->setContentsMargins(horizontalMargin, verticalMargin, horizontalMargin, verticalMargin);
        auto *action = new QWidgetAction(this);
        action->setDefaultWidget(widget);
        addAction(action);
    }
}

void ImageAdjustmentMenu::addFitOptions()
{
    enlargeImagesAction = addAction(QCoreApplication::translate("OptionsDialog", "Enlarge images to fit width/height"));
    enlargeImagesAction->setCheckable(true);
    connect(enlargeImagesAction, &QAction::triggered, this, [](bool checked) {
        Configuration::getConfiguration().setEnlargeImages(checked);
    });

    auto *continuousScrollMenu = addMenu(QCoreApplication::translate("OptionsDialog", "Continuous scroll"));
    auto *continuousScrollFit = new QActionGroup(this);
    continuousScrollFitToWidthAction = continuousScrollMenu->addAction(QCoreApplication::translate("OptionsDialog", "Fit pages to width"));
    continuousScrollFitToHeightAction = continuousScrollMenu->addAction(QCoreApplication::translate("OptionsDialog", "Fit pages to height"));
    for (auto *action : { continuousScrollFitToWidthAction, continuousScrollFitToHeightAction }) {
        action->setCheckable(true);
        continuousScrollFit->addAction(action);
    }
    connect(continuousScrollFit, &QActionGroup::triggered, this, [this](QAction *action) {
        Configuration::getConfiguration().setContinuousScrollFitToHeight(action == continuousScrollFitToHeightAction);
    });
}

void ImageAdjustmentMenu::addScalingMethods()
{
    auto *scalingMenu = addMenu(QCoreApplication::translate("OptionsDialog", "Scaling method"));
    scalingMethods = new QActionGroup(this);
    for (const auto &choice : scalingMethodChoices()) {
        auto *action = scalingMenu->addAction(choice.text());
        action->setCheckable(true);
        // the stored value, as the Options combo items hold it, since it is not the position
        action->setData(static_cast<int>(choice.method));
        scalingMethods->addAction(action);
    }
    connect(scalingMethods, &QActionGroup::triggered, this, [](QAction *action) {
        Configuration::getConfiguration().setScalingMethod(static_cast<ScaleMethod>(action->data().toInt()));
    });
}

void ImageAdjustmentMenu::showStoredOptions()
{
    auto &configuration = Configuration::getConfiguration();

    enlargeImagesAction->setChecked(configuration.getEnlargeImages());
    if (configuration.getContinuousScrollFitToHeight())
        continuousScrollFitToHeightAction->setChecked(true);
    else
        continuousScrollFitToWidthAction->setChecked(true);

    const int scalingMethod = static_cast<int>(configuration.getScalingMethod());
    for (auto *action : scalingMethods->actions()) {
        if (action->data().toInt() == scalingMethod)
            action->setChecked(true);
    }
}
