#include "page_prescaler.h"

#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <limits>
#include <mutex>
#include <utility>

namespace {

// Larger than the number of views, so every view's first stage runs before any view's second one
constexpr int kStagePriorityStep = 1000;

qsizetype imageBytes(QSize pixelSize)
{
    return qsizetype(pixelSize.width()) * pixelSize.height() * 4;
}

bool hasTransparentPixels(const QImage &image)
{
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        if (std::any_of(line, line + image.width(), [](QRgb pixel) { return qAlpha(pixel) != 255; })) {
            return true;
        }
    }
    return false;
}

// In the formats raster pixmaps use, so QPixmap::fromImage on the GUI thread shares the pixels instead of
// converting them
QImage displayReady(QImage image, qreal devicePixelRatio)
{
    if (image.isNull()) {
        return image;
    }

    if (!image.hasAlphaChannel()) {
        image.convertTo(QImage::Format_RGB32);
    } else {
        if (image.format() != QImage::Format_ARGB32) {
            image.convertTo(QImage::Format_ARGB32_Premultiplied);
        }
        // an opaque ARGB32 image already is RGB32, so it needs no premultiplying
        if (!hasTransparentPixels(image)) {
            image.reinterpretAsFormat(QImage::Format_RGB32);
        } else {
            image.convertTo(QImage::Format_ARGB32_Premultiplied);
        }
    }
    image.setDevicePixelRatio(devicePixelRatio);
    return image;
}

}

// Drawing a double page at full resolution takes a while, so the stages of a view share it
struct PagePrescaler::Source {
    explicit Source(PageViewImages images)
        : images(std::move(images))
    {
    }

    // called on several threads
    const QImage &image()
    {
        std::call_once(drawn, [this] { fullImage = images.image(); });
        return fullImage;
    }

    const PageViewImages images;
    std::once_flag drawn;
    QImage fullImage;
};

bool PrescaleStage::isActive(QSize, QSize) const
{
    return true;
}

int PrescaleStage::maxConcurrent() const
{
    return std::numeric_limits<int>::max();
}

QImage ScaleStage::process(const QImage &image, QSize pixelSize, ScaleMethod scaleMethod, const std::atomic_bool &) const
{
    return scaleImage(image, pixelSize.width(), pixelSize.height(), scaleMethod);
}

int PagePrescaler::defaultThreadCount()
{
    return std::clamp(QThread::idealThreadCount() / 4, 1, 2);
}

PagePrescaler::PagePrescaler(QList<std::shared_ptr<const PrescaleStage>> stages, int maxThreads, qsizetype maxBytes, QObject *parent)
    : QObject(parent), stages(std::move(stages)), maxBytes(maxBytes), busy(this->stages.size(), 0)
{
    Q_ASSERT(!this->stages.isEmpty());
    pool.setMaxThreadCount(maxThreads);
}

PagePrescaler::~PagePrescaler()
{
    invalidate();
    pool.waitForDone();
}

QImage PagePrescaler::image(const PrescaleKey &key) const
{
    const auto view = views.constFind(key);
    return view != views.cend() ? view->image : QImage();
}

QImage PagePrescaler::scaleNow(const PrescaleRequest &request)
{
    if (request.key.pixelSize.isEmpty()) {
        return QImage();
    }

    // a window being resized asks for a new size at every step, so only the last view outside the schedule is kept
    views.removeIf([this, &request](QHash<PrescaleKey, View>::iterator view) {
        return view.key() != request.key && !isRequested(view.key());
    });

    View &view = views[request.key];
    if (!view.image.isNull()) {
        return view.image;
    }

    if (view.job && view.job->stage == 0) {
        cancel(view);
    }
    if (!view.source) {
        view.source = std::make_shared<Source>(request.images);
    }
    const std::atomic_bool notCancelled = false;
    view.image = displayReady(stages.first()->process(view.source->image(), request.key.pixelSize, request.key.scaleMethod, notCancelled),
                              request.key.devicePixelRatio);
    view.stage = std::max(view.stage, 0);
    return view.image;
}

void PagePrescaler::schedule(const QList<PrescaleRequest> &newRequests)
{
    requests.clear();
    qsizetype bytes = 0;
    for (const PrescaleRequest &request : newRequests) {
        bytes += imageBytes(request.key.pixelSize);
        if (!requests.isEmpty() && bytes > maxBytes) {
            break;
        }
        requests.append(request);
    }

    // what has not started yet is queued again below, in the new order
    views.removeIf([this](QHash<PrescaleKey, View>::iterator view) {
        if (!isRequested(view.key())) {
            cancel(*view);
            return true;
        }
        takeBack(*view);
        return false;
    });
    // every job left in the queue has been taken back
    pool.clear();

    startMissingStages();
}

void PagePrescaler::invalidate()
{
    for (View &view : views) {
        cancel(view);
    }
    pool.clear();
    views.clear();
    requests.clear();
}

bool PagePrescaler::isRequested(const PrescaleKey &key) const
{
    return std::any_of(requests.cbegin(), requests.cend(), [&key](const PrescaleRequest &request) { return request.key == key; });
}

// The first stage after `after` that does not decline the view, or the number of stages when none is left
int PagePrescaler::nextStage(const PrescaleRequest &request, int after) const
{
    const QSize imageSize = request.images.size();
    int stage = after + 1;
    while (stage < stages.size() && !stages.at(stage)->isActive(imageSize, request.key.pixelSize)) {
        ++stage;
    }
    return stage;
}

void PagePrescaler::startMissingStages()
{
    for (int order = 0; order < requests.size(); ++order) {
        const PrescaleRequest &request = requests.at(order);
        View &view = views[request.key];
        if (view.job) {
            continue;
        }

        const int stage = nextStage(request, view.stage);
        if (stage == stages.size()) {
            view.source.reset();
        } else if (busy.at(stage) < stages.at(stage)->maxConcurrent()) {
            if (!view.source) {
                view.source = std::make_shared<Source>(request.images);
            }
            start(request.key, view, stage, order);
        }
    }
}

void PagePrescaler::start(const PrescaleKey &key, View &view, int stage, int order)
{
    auto job = std::make_shared<Job>();
    job->stage = stage;
    view.job = job;
    ++busy[stage];

    const auto run = [this, key, job, source = view.source, step = stages.at(stage)] {
        if (job->claimed.exchange(true)) {
            return;
        }

        QImage image;
        if (!job->cancelled) {
            image = step->process(source->image(), key.pixelSize, key.scaleMethod, job->cancelled);
        }
        if (!job->cancelled) {
            image = displayReady(std::move(image), key.devicePixelRatio);
        }
        QMetaObject::invokeMethod(this, [this, key, job, image] { finish(key, job, image); }, Qt::QueuedConnection);
    };
    pool.start(run, -(stage * kStagePriorityStep + order));
}

// Makes sure a job that has not started never will
bool PagePrescaler::takeBack(View &view)
{
    if (!view.job || view.job->claimed.exchange(true)) {
        return false;
    }
    view.job->cancelled = true;
    --busy[view.job->stage];
    view.job.reset();
    return true;
}

// A job that is running finishes on its own, and its result is dropped
void PagePrescaler::cancel(View &view)
{
    if (!takeBack(view) && view.job) {
        view.job->cancelled = true;
        view.job.reset();
    }
}

// Every job that ran ends here. A stage that fails keeps the image of the one before.
void PagePrescaler::finish(const PrescaleKey &key, const std::shared_ptr<Job> &job, const QImage &image)
{
    --busy[job->stage];
    const bool improved = !job->cancelled && !image.isNull();
    if (!job->cancelled) {
        View &view = views[key];
        view.job.reset();
        view.stage = job->stage;
        if (improved) {
            view.image = image;
        }
    }

    startMissingStages();
    if (improved) {
        emit imageImproved(key);
    }
}
