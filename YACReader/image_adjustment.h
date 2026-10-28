#ifndef IMAGE_ADJUSTMENT_H
#define IMAGE_ADJUSTMENT_H

#include "resize_image.h"

#include <QList>
#include <QString>

class QComboBox;

struct ScalingMethodChoice {
    ScaleMethod method;
    const char *label; // source text in the OptionsDialog translation context

    QString text() const;
};

// In the order Options > Image adjustment lists them
const QList<ScalingMethodChoice> &scalingMethodChoices();

// The combo items hold their methods as data, because from Lanczos3 on a method's position is not its stored value
void addScalingMethodItems(QComboBox *combo);
void selectScalingMethod(QComboBox *combo, ScaleMethod method);
ScaleMethod selectedScalingMethod(const QComboBox *combo);

#endif // IMAGE_ADJUSTMENT_H
