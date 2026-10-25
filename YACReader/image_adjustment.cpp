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
        { ScaleMethod::Lanczos4, QT_TRANSLATE_NOOP("OptionsDialog", "Lanczos4 (more detail, more halos)") },
        { ScaleMethod::Lanczos3, QT_TRANSLATE_NOOP("OptionsDialog", "Lanczos3 (balanced)") },
        { ScaleMethod::Lanczos2, QT_TRANSLATE_NOOP("OptionsDialog", "Lanczos2 (fewer halos, less detail)") },
    };
}

int scalingMethodChoiceIndex(const QList<ScalingMethodChoice> &choices, ScaleMethod method)
{
    const auto choice = std::find_if(choices.cbegin(), choices.cend(), [method](const ScalingMethodChoice &choice) {
        return choice.method == method;
    });
    return choice == choices.cend() ? -1 : static_cast<int>(std::distance(choices.cbegin(), choice));
}
