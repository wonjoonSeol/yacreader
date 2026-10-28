#ifndef IMAGE_FILTER_SLIDER_H
#define IMAGE_FILTER_SLIDER_H

#include "configuration.h"

#include <QList>
#include <QTimer>
#include <QWidget>

#include <chrono>

class QLabel;
class YACReaderSpinSliderWidget;

// A brightness, contrast or gamma slider with its label. It shows the stored level, and stores a new
// one once it has stopped changing for storeDelay, so a run of spin box steps or wheel notches filters
// the pages and writes the settings once.
class ImageFilterSlider : public QWidget
{
    Q_OBJECT
public:
    ImageFilterSlider(QLabel *label, const ImageFilterSpec &filter, QWidget *parent = nullptr);

    static constexpr std::chrono::milliseconds storeDelay { 150 };

private:
    void showStoredLevel();

    const ImageFilterSpec filter;
    YACReaderSpinSliderWidget *slider;
    QTimer storeTimer;
};

// The sliders and Reset of Options > Image adjustment > Image options, top to bottom. The image
// adjustment menu shows them too, each as an entry of its own: QMenu moves the keyboard focus from
// entry to entry, so a single entry for all of them would only reach its first slider.
QList<QWidget *> createImageFilterWidgets();

#endif // IMAGE_FILTER_SLIDER_H
