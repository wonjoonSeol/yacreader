#ifndef IMAGE_ADJUSTMENT_MENU_H
#define IMAGE_ADJUSTMENT_MENU_H

#include <QList>
#include <QMenu>

class QAction;

// The options of Options > Image adjustment that are worth changing while reading. Like Options, it
// stores them in Configuration and shows what Configuration reports, so the two always agree.
class ImageAdjustmentMenu : public QMenu
{
    Q_OBJECT
public:
    explicit ImageAdjustmentMenu(QWidget *parent = nullptr);

private:
    void addFilterSliders();
    void addFitOptions();
    void addScalingMethods();
    void showStoredOptions();

    QAction *enlargeImagesAction;
    QAction *continuousScrollFitToWidthAction;
    QAction *continuousScrollFitToHeightAction;
    QList<QAction *> scalingMethodActions;
};

#endif // IMAGE_ADJUSTMENT_MENU_H
