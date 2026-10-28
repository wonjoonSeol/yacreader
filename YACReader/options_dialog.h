#ifndef __OPTIONS_DIALOG_H
#define __OPTIONS_DIALOG_H

#include "themable.h"
#include "yacreader_options_dialog.h"

#include <QPointer>

class QComboBox;
class QDialog;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QPushButton;
class QRadioButton;

class OptionsDialog : public YACReaderOptionsDialog, protected Themable
{
    Q_OBJECT
public:
    OptionsDialog(QWidget *parent = nullptr);

protected:
    void applyTheme(const Theme &theme) override;

private:
    // QLabel * pathLabel;
    QLineEdit *pathEdit;
    QPushButton *pathFindButton;
    QComboBox *languageCombo;

    QCheckBox *showTimeInInformationLabel;
    QCheckBox *showPageCounter;
    QRadioButton *openAtLastReadPageRadioButton;
    QRadioButton *openAtFirstPageIfFinishedRadioButton;
    QRadioButton *openAtFirstPageRadioButton;

    QCheckBox *circularMagnifyingGlass;
    QCheckBox *magnifyingGlassRing;
    QCheckBox *magnifyingGlassEdgeEase;

    QCheckBox *useSoftwareGoToFlow;
    QCheckBox *quickNavi;
    QCheckBox *disableShowOnMouseOver;
    QCheckBox *scaleCheckbox;
    QCheckBox *coverSPCheckBox;
    QCheckBox *doublePageShadowCheckBox;
    QRadioButton *continuousScrollFitToWidthRadioButton;
    QRadioButton *continuousScrollFitToHeightRadioButton;

    QLabel *magGlassSizeLabel;

    QLabel *zoomLevel;

    // QLabel * slideSizeLabel;
    QSlider *slideSize;

    // QLabel * fitToWidthRatioLabel;
    // QSlider * fitToWidthRatioS;

    QLabel *backgroundColor;
    QPushButton *selectBackgroundColorButton;
    QPushButton *clearBackgroundColorButton;

    QCheckBox *doNotTurnPageOnScroll;
    QCheckBox *useSingleScrollStepToTurnPage;
    QCheckBox *disableScrollAnimations;

    QColor currentColor;
    bool backgroundColorFollowsTheme = true;

    QComboBox *scalingMethodCombo;
    QCheckBox *acnetUpscalingCheckBox;
    QComboBox *acnetModelCombo;

    QRadioButton *normalMouseModeRadioButton;
    QRadioButton *leftRightNavigationMouseModeRadioButton;
    QRadioButton *hotAreasMouseModeRadioButton;

    QRadioButton *escapeQuitsRadioButton;
    QRadioButton *escapeCancelsModeRadioButton;

    // The Image adjustment page except brightness, contrast and gamma, which show the stored levels themselves
    void reloadImageOptions();

public slots:
    void saveOptions() override;
    void restoreOptions(QSettings *settings) override;
    void findFolder();
    void showColorDialog();
    void updateColor(const QColor &color);
    // void fitToWidthRatio(int value);
    void show();
    void clearBackgroundColor();

signals:
    void changedOptions();
    // the cover or book fold option of Double page changed; Configuration reports the other image options
    void changedImageOptions();
    // void fitToWidthRatioChanged(float ratio);
};

#endif
