#ifndef ACNET_STAGE_H
#define ACNET_STAGE_H

#include "acnet_upscale.h"
#include "page_prescaler.h"

#include <atomic>
#include <optional>

// The prescaler stage after the plain scale: a view that ACNet enlarges (see acnetUpscales) is upscaled with it, and
// the sharper image replaces the plain one when it is ready. Declines every view until a model is set.
class AcnetStage : public PrescaleStage
{
public:
    // Called on the GUI thread. Returns whether the images it made before are now wrong, so the prescaler has to be
    // invalidated; turning it on needs nothing, since the prescaler asks isActive() again at every schedule.
    bool setModel(std::optional<AcnetModel> model)
    {
        const bool stale = enabled && model != acnetModel.load();
        enabled = model.has_value();
        if (model) {
            acnetModel = *model;
        }
        return stale;
    }

    bool isActive(QSize imageSize, QSize pixelSize) const override
    {
        return enabled && acnetUpscales(imageSize, pixelSize);
    }

    // One upscale already runs on all the threads ACNet may take
    int maxConcurrent() const override { return 1; }

    QImage process(const QImage &image, QSize pixelSize, ScaleMethod scaleMethod, const std::atomic_bool &cancelled) const override
    {
        return scaleImageWithAcnet(image, pixelSize, scaleMethod, acnetModel, cancelled);
    }

private:
    std::atomic_bool enabled = false;
    std::atomic<AcnetModel> acnetModel = AcnetModel::F8B4;
};

#endif // ACNET_STAGE_H
