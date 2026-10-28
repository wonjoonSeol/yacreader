#ifndef ACNET_STAGE_H
#define ACNET_STAGE_H

#include "acnet_upscale.h"
#include "page_prescaler.h"

#include <atomic>
#include <optional>

// The prescaler stage after the plain scale: a page that ACNet enlarges (see acnetUpscales) is upscaled with it, and
// the sharper image replaces the plain one when it is ready. Does nothing until a model is set.
class AcnetStage : public PrescaleStage
{
public:
    // Called on the GUI thread. Returns whether the model changed; the images already prepared stay until the
    // prescaler is invalidated.
    bool setModel(std::optional<AcnetModel> model)
    {
        const bool changed = model.has_value() != enabled || (model && *model != acnetModel);
        enabled = model.has_value();
        if (model) {
            acnetModel = *model;
        }
        return changed;
    }

    QImage process(const QImage &image, QSize pixelSize, ScaleMethod scaleMethod) const override
    {
        const std::atomic_bool notCancelled = false;
        return enabled ? scaleImageWithAcnet(image, pixelSize, scaleMethod, acnetModel, notCancelled) : QImage();
    }

private:
    std::atomic_bool enabled = false;
    std::atomic<AcnetModel> acnetModel = AcnetModel::F8B4;
};

#endif // ACNET_STAGE_H
