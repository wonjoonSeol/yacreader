#include "image_adjustment_menu.h"

#include "configuration.h"
#include "image_adjustment.h"
#include "image_filter_slider.h"

#include <QActionGroup>
#include <QCoreApplication>
#include <QStyle>
#include <QWidgetAction>

// The labels are those of Options, in its translation context, so both read the same in every language

namespace {

// A checkable entry of an exclusive group, holding the value it stores
void addChoice(QMenu *menu, QActionGroup *group, const QString &text, const QVariant &value)
{
    auto *action = menu->addAction(text);
    action->setCheckable(true);
    action->setData(value);
    group->addAction(action);
}

void checkChoice(QActionGroup *group, const QVariant &value)
{
    for (auto *action : group->actions()) {
        if (action->data() == value)
            action->setChecked(true);
    }
}

}

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
        // QMenu gives an entry's widget the keyboard focus only when it accepts focus
        widget->setFocusPolicy(Qt::TabFocus);
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
    continuousScrollFit = new QActionGroup(this);
    addChoice(continuousScrollMenu, continuousScrollFit, QCoreApplication::translate("OptionsDialog", "Fit pages to width"), false);
    addChoice(continuousScrollMenu, continuousScrollFit, QCoreApplication::translate("OptionsDialog", "Fit pages to height"), true);
    connect(continuousScrollFit, &QActionGroup::triggered, this, [](QAction *action) {
        Configuration::getConfiguration().setContinuousScrollFitToHeight(action->data().toBool());
    });
}

void ImageAdjustmentMenu::addScalingMethods()
{
    auto *scalingMenu = addMenu(QCoreApplication::translate("OptionsDialog", "Scaling method"));
    scalingMethods = new QActionGroup(this);
    // the stored value, as the Options combo items hold it, since it is not the position
    for (const auto &choice : scalingMethodChoices())
        addChoice(scalingMenu, scalingMethods, choice.text(), static_cast<int>(choice.method));
    connect(scalingMethods, &QActionGroup::triggered, this, [](QAction *action) {
        Configuration::getConfiguration().setScalingMethod(static_cast<ScaleMethod>(action->data().toInt()));
    });
}

void ImageAdjustmentMenu::showStoredOptions()
{
    auto &configuration = Configuration::getConfiguration();

    enlargeImagesAction->setChecked(configuration.getEnlargeImages());
    checkChoice(continuousScrollFit, configuration.getContinuousScrollFitToHeight());
    checkChoice(scalingMethods, static_cast<int>(configuration.getScalingMethod()));
}
