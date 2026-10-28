#include "image_filter_slider.h"

#include "yacreader_spin_slider_widget.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

#include <algorithm>

ImageFilterSlider::ImageFilterSlider(QLabel *label, const ImageFilterSpec &filter, QWidget *parent)
    : QWidget(parent), filter(filter)
{
    slider = new YACReaderSpinSliderWidget(nullptr, true);
    slider->layout()->setContentsMargins(0, 0, 0, 0);
    slider->setRange(filter.minimum, filter.maximum);
    slider->setTracking(false);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(label);
    layout->addWidget(slider, 1);

    // a menu entry gets the keyboard focus only when its widget accepts it
    setFocusPolicy(Qt::TabFocus);
    setFocusProxy(slider);

    storeTimer.setSingleShot(true);
    storeTimer.setInterval(storeDelay);
    connect(slider, &YACReaderSpinSliderWidget::valueChanged, &storeTimer, qOverload<>(&QTimer::start));
    connect(&storeTimer, &QTimer::timeout, this, [this] {
        Configuration::getConfiguration().setFilterLevel(this->filter, slider->getValue());
    });

    connect(&Configuration::getConfiguration(), &Configuration::filtersChanged, this, &ImageFilterSlider::showStoredLevel);
    showStoredLevel();
}

void ImageFilterSlider::showStoredLevel()
{
    const QSignalBlocker blocker(slider);
    slider->setValue(Configuration::getConfiguration().getFilterLevel(filter));
}

QList<QWidget *> createImageFilterWidgets()
{
    // the labels keep the translation context of Options, where they were first
    const std::pair<QString, ImageFilterSpec> filters[] = {
        { QCoreApplication::translate("OptionsDialog", "Brightness"), brightnessSpec },
        { QCoreApplication::translate("OptionsDialog", "Contrast"), contrastSpec },
        { QCoreApplication::translate("OptionsDialog", "Gamma"), gammaSpec },
    };

    QList<QWidget *> widgets;
    QList<QLabel *> labels;
    for (const auto &[text, filter] : filters) {
        auto *label = new QLabel(text);
        labels << label;
        widgets << new ImageFilterSlider(label, filter);
    }
    // each row has its own layout, so the sliders line up only if the labels are as wide
    int labelWidth = 0;
    for (auto *label : std::as_const(labels))
        labelWidth = std::max(labelWidth, label->sizeHint().width());
    for (auto *label : std::as_const(labels))
        label->setMinimumWidth(labelWidth);

    auto *reset = new QWidget();
    auto *resetLayout = new QHBoxLayout(reset);
    resetLayout->setContentsMargins(0, 0, 0, 0);
    auto *resetButton = new QPushButton(QCoreApplication::translate("OptionsDialog", "Reset"));
    resetLayout->addWidget(resetButton);
    resetLayout->addStretch();
    reset->setFocusPolicy(Qt::TabFocus);
    reset->setFocusProxy(resetButton);
    QObject::connect(resetButton, &QAbstractButton::clicked, resetButton, [] {
        Configuration::getConfiguration().setFilterLevels(brightnessSpec.defaultValue, contrastSpec.defaultValue, gammaSpec.defaultValue);
    });
    widgets << reset;

    return widgets;
}
