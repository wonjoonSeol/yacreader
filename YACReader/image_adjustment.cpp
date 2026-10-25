#include "image_adjustment.h"

#include <QCoreApplication>

#include <algorithm>

QString ScalingMethodChoice::text() const
{
    return QCoreApplication::translate("OptionsDialog", label);
}

QList<ScalingMethodChoice> scalingMethodChoices()
{
    return {
        { ScaleMethod::Nearest, QT_TRANSLATE_NOOP("OptionsDialog", "Nearest (fast, low quality)") },
        { ScaleMethod::Bilinear, QT_TRANSLATE_NOOP("OptionsDialog", "Bilinear") },
        { ScaleMethod::Lanczos, QT_TRANSLATE_NOOP("OptionsDialog", "Lanczos (better quality)") },
    };
}

int scalingMethodChoiceIndex(const QList<ScalingMethodChoice> &choices, ScaleMethod method)
{
    const auto choice = std::find_if(choices.cbegin(), choices.cend(), [method](const ScalingMethodChoice &choice) {
        return choice.method == method;
    });
    return choice == choices.cend() ? -1 : static_cast<int>(std::distance(choices.cbegin(), choice));
}
