#ifndef IMAGE_ADJUSTMENT_MENU_H
#define IMAGE_ADJUSTMENT_MENU_H

#include <QList>
#include <QMenu>

class QAction;
class YACReaderSpinSliderWidget;

// The options of Options > Image adjustment that are worth changing while reading.
// It shows the stored settings each time it opens, so it agrees with the Options dialog.
class ImageAdjustmentMenu : public QMenu
{
    Q_OBJECT
public:
    explicit ImageAdjustmentMenu(QWidget *parent = nullptr);

signals:
    // a fit, continuous scroll or scaling option was stored
    void imageOptionsChanged();
    void filtersChanged(int brightness, int contrast, int gamma);
    void resetFiltersRequested();

private:
    void addFilterSliders();
    void addFitOptions();
    void addScalingMethods();
    void showStoredOptions();
    void showStoredFilters();

    YACReaderSpinSliderWidget *brightnessSlider;
    YACReaderSpinSliderWidget *contrastSlider;
    YACReaderSpinSliderWidget *gammaSlider;
    QAction *enlargeImagesAction;
    QAction *continuousScrollFitToWidthAction;
    QAction *continuousScrollFitToHeightAction;
    QList<QAction *> scalingMethodActions;
};

#endif // IMAGE_ADJUSTMENT_MENU_H
