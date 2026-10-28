#include "image_adjustment.h"

#include <QComboBox>
#include <QCoreApplication>

QString ScalingMethodChoice::text() const
{
    return QCoreApplication::translate("OptionsDialog", label);
}

const QList<ScalingMethodChoice> &scalingMethodChoices()
{
    static const QList<ScalingMethodChoice> choices {
        { ScaleMethod::Nearest, QT_TRANSLATE_NOOP("OptionsDialog", "Nearest (fast, low quality)") },
        { ScaleMethod::Bilinear, QT_TRANSLATE_NOOP("OptionsDialog", "Bilinear") },
        { ScaleMethod::Lanczos, QT_TRANSLATE_NOOP("OptionsDialog", "Lanczos4 (more detail, more halos)") },
        { ScaleMethod::Lanczos3, QT_TRANSLATE_NOOP("OptionsDialog", "Lanczos3 (balanced)") },
        { ScaleMethod::Lanczos2, QT_TRANSLATE_NOOP("OptionsDialog", "Lanczos2 (fewer halos, less detail)") },
    };
    return choices;
}

void addScalingMethodItems(QComboBox *combo)
{
    for (const auto &choice : scalingMethodChoices())
        combo->addItem(choice.text(), static_cast<int>(choice.method));
}

void selectScalingMethod(QComboBox *combo, ScaleMethod method)
{
    combo->setCurrentIndex(combo->findData(static_cast<int>(method)));
}

ScaleMethod selectedScalingMethod(const QComboBox *combo)
{
    return static_cast<ScaleMethod>(combo->currentData().toInt());
}
