#ifndef IMAGE_ADJUSTMENT_H
#define IMAGE_ADJUSTMENT_H

#include "resize_image.h"

#include <QList>
#include <QString>

struct ScalingMethodChoice {
    ScaleMethod method;
    const char *label; // source text in the OptionsDialog translation context

    QString text() const;
};

// In the order Options > Image adjustment lists them, which is not the order of the stored values.
// A new method only needs an entry here.
QList<ScalingMethodChoice> scalingMethodChoices();

// The position of the choice that stores method, or -1 if no choice does
int scalingMethodChoiceIndex(const QList<ScalingMethodChoice> &choices, ScaleMethod method);

#endif // IMAGE_ADJUSTMENT_H
