#include "image_filter_slider.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

#include <algorithm>

ImageFilterSlider::ImageFilterSlider(const QString &text, const ImageFilterSpec &filter, QWidget *parent)
    : YACReaderSpinSliderWidget(parent, true), filter(filter)
{
    layout()->setContentsMargins(0, 0, 0, 0);
    setText(text);
    setRange(filter.minimum, filter.maximum);
    setTracking(false);

    storeTimer.setSingleShot(true);
    storeTimer.setInterval(storeDelay);
    connect(this, &YACReaderSpinSliderWidget::valueChanged, &storeTimer, qOverload<>(&QTimer::start));
    connect(&storeTimer, &QTimer::timeout, this, [this] {
        Configuration::getConfiguration().setFilterLevel(this->filter, getValue());
    });

    connect(&Configuration::getConfiguration(), &Configuration::filtersChanged, this, &ImageFilterSlider::showStoredLevel);
    showStoredLevel();
}

void ImageFilterSlider::alignLabels(const QList<ImageFilterSlider *> &sliders)
{
    int labelWidth = 0;
    for (auto *slider : sliders)
        labelWidth = std::max(labelWidth, slider->label->sizeHint().width());
    for (auto *slider : sliders)
        slider->label->setMinimumWidth(labelWidth);
}

void ImageFilterSlider::showStoredLevel()
{
    const QSignalBlocker blocker(this);
    setValue(Configuration::getConfiguration().getFilterLevel(filter));
}

QList<QWidget *> createImageFilterWidgets()
{
    // the labels keep the translation context of Options, where they were first
    const QList<ImageFilterSlider *> sliders {
        new ImageFilterSlider(QCoreApplication::translate("OptionsDialog", "Brightness"), brightnessSpec),
        new ImageFilterSlider(QCoreApplication::translate("OptionsDialog", "Contrast"), contrastSpec),
        new ImageFilterSlider(QCoreApplication::translate("OptionsDialog", "Gamma"), gammaSpec),
    };
    ImageFilterSlider::alignLabels(sliders);

    auto *reset = new QWidget();
    auto *resetLayout = new QHBoxLayout(reset);
    resetLayout->setContentsMargins(0, 0, 0, 0);
    auto *resetButton = new QPushButton(QCoreApplication::translate("OptionsDialog", "Reset"));
    resetLayout->addWidget(resetButton);
    resetLayout->addStretch();
    reset->setFocusProxy(resetButton);
    QObject::connect(resetButton, &QAbstractButton::clicked, resetButton, [] {
        Configuration::getConfiguration().setFilterLevels(brightnessSpec.defaultValue, contrastSpec.defaultValue, gammaSpec.defaultValue);
    });

    QList<QWidget *> widgets(sliders.cbegin(), sliders.cend());
    widgets << reset;
    return widgets;
}
