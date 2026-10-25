#ifndef ACNET_STAGE_H
#define ACNET_STAGE_H

#include "acnet_upscale.h"
#include "page_prescaler.h"

#include <atomic>

// The prescaler stage after the plain scale: a page shown larger than its size is upscaled with ACNet, and the
// sharper image replaces the plain one when it is ready. Does nothing until it is enabled.
class AcnetStage : public PrescaleStage
{
public:
    // Called on the GUI thread. Images already prepared stay until the prescaler is invalidated.
    void setOptions(bool enabled, AcnetModel model)
    {
        this->enabled = enabled;
        acnetModel = model;
    }
    bool isEnabled() const { return enabled; }
    AcnetModel model() const { return acnetModel; }

    QImage process(const QImage &image, QSize pixelSize, ScaleMethod scaleMethod) const override
    {
        return enabled ? scaleImageWithAcnet(image, pixelSize, scaleMethod, acnetModel) : QImage();
    }

private:
    std::atomic_bool enabled = false;
    std::atomic<AcnetModel> acnetModel = AcnetModel::F8B4;
};

#endif // ACNET_STAGE_H
