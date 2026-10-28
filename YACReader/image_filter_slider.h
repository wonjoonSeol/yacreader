#ifndef IMAGE_FILTER_SLIDER_H
#define IMAGE_FILTER_SLIDER_H

#include "configuration.h"
#include "yacreader_spin_slider_widget.h"

#include <QList>
#include <QTimer>

#include <chrono>

// A brightness, contrast or gamma slider with its label. It shows the stored level, and stores a new
// one once it has stopped changing for storeDelay, so a run of spin box steps or wheel notches filters
// the pages and writes the settings once.
class ImageFilterSlider : public YACReaderSpinSliderWidget
{
    Q_OBJECT
public:
    ImageFilterSlider(const QString &text, const ImageFilterSpec &filter, QWidget *parent = nullptr);

    static constexpr std::chrono::milliseconds storeDelay { 150 };

    // Sliders in layouts of their own line up only when their labels are as wide
    static void alignLabels(const QList<ImageFilterSlider *> &sliders);

private:
    void showStoredLevel();

    const ImageFilterSpec filter;
    QTimer storeTimer;
};

// The sliders and Reset of Options > Image adjustment > Image options, top to bottom. The image
// adjustment menu shows them too, each as an entry of its own: QMenu moves the keyboard focus from
// entry to entry, so a single entry for all of them would only reach its first slider.
QList<QWidget *> createImageFilterWidgets();

#endif // IMAGE_FILTER_SLIDER_H
