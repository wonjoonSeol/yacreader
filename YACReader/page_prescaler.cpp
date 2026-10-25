#include "page_prescaler.h"

#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <utility>

namespace {

// Larger than the number of views, so every view's first stage runs before any view's second one
constexpr int kStagePriorityStep = 1000;

// Render decodes one page at a time, so a couple of threads next to it keep most cores free
int defaultThreadCount()
{
    return std::clamp(QThread::idealThreadCount() / 4, 1, 2);
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
        image.convertTo(QImage::Format_ARGB32_Premultiplied);
        if (!hasTransparentPixels(image)) {
            image.reinterpretAsFormat(QImage::Format_RGB32);
        }
    }
    image.setDevicePixelRatio(devicePixelRatio);
    return image;
}

}

QImage ScaleStage::process(const QImage &image, QSize pixelSize, ScaleMethod scaleMethod) const
{
    return scaleImage(image, pixelSize.width(), pixelSize.height(), scaleMethod);
}

PagePrescaler::PagePrescaler(QObject *parent)
    : PagePrescaler({ std::make_shared<ScaleStage>() }, defaultThreadCount(), parent)
{
}

PagePrescaler::PagePrescaler(QList<std::shared_ptr<const PrescaleStage>> stages, int maxThreads, QObject *parent)
    : QObject(parent), stages(std::move(stages))
{
    pool.setMaxThreadCount(maxThreads);
}

PagePrescaler::~PagePrescaler()
{
    invalidate();
    pool.waitForDone();
}

QImage PagePrescaler::image(const PrescaleKey &key) const
{
    const auto entry = entries.constFind(key.view.firstPage);
    return entry != entries.cend() && entry->key == key ? entry->image : QImage();
}

void PagePrescaler::store(const PrescaleKey &key, const QImage &image)
{
    keep(key, image, 0);

    const auto job = jobs.constFind(key.view.firstPage);
    if (job != jobs.cend() && job->key == key && job->stage == 0) {
        job->state->cancelled = true;
        jobs.erase(job);
    }
}

void PagePrescaler::schedule(const QList<PrescaleRequest> &newRequests)
{
    requests = newRequests;

    // what has not started yet is started again below, in the new order
    pool.clear();
    for (auto job = jobs.begin(); job != jobs.end();) {
        if (!job->state->started || !isRequested(job->key)) {
            job->state->cancelled = true;
            job = jobs.erase(job);
        } else {
            ++job;
        }
    }
    entries.removeIf([this](QHash<int, Entry>::iterator entry) { return !isRequested(entry->key); });

    startMissingStages();
}

void PagePrescaler::invalidate()
{
    ++generation;
    pool.clear();
    for (const Job &job : std::as_const(jobs)) {
        job.state->cancelled = true;
    }
    jobs.clear();
    entries.clear();
    requests.clear();
}

bool PagePrescaler::isRequested(const PrescaleKey &key) const
{
    return std::any_of(requests.cbegin(), requests.cend(), [&key](const PrescaleRequest &request) { return request.key == key; });
}

// A result replaces the view's image when it comes from a later stage. A stage that fails leaves the image of the
// one before.
bool PagePrescaler::keep(const PrescaleKey &key, const QImage &image, int stage)
{
    const auto entry = entries.find(key.view.firstPage);
    if (entry == entries.end() || entry->key != key) {
        entries.insert(key.view.firstPage, { key, image, stage });
        return !image.isNull();
    }

    if (stage <= entry->stage) {
        return false;
    }
    entry->stage = stage;
    if (image.isNull()) {
        return false;
    }
    entry->image = image;
    return true;
}

void PagePrescaler::startMissingStages()
{
    for (int order = 0; order < requests.size(); ++order) {
        const PrescaleRequest &request = requests.at(order);
        if (jobs.contains(request.key.view.firstPage)) {
            continue;
        }

        const auto entry = entries.constFind(request.key.view.firstPage);
        const int stage = entry != entries.cend() && entry->key == request.key ? entry->stage + 1 : 0;
        if (stage < stages.size()) {
            start(request, stage, order);
        }
    }
}

void PagePrescaler::start(const PrescaleRequest &request, int stage, int order)
{
    auto state = std::make_shared<JobState>();
    jobs.insert(request.key.view.firstPage, { request.key, stage, state });

    const quint64 jobGeneration = generation;
    const auto run = [this, request, stage, jobGeneration, state, step = stages.at(stage)] {
        state->started = true;
        if (state->cancelled) {
            return;
        }

        const QImage image = displayReady(step->process(request.images.image(), request.key.pixelSize, request.key.scaleMethod),
                                          request.key.devicePixelRatio);
        if (state->cancelled) {
            return;
        }

        QMetaObject::invokeMethod(
                this, [this, key = request.key, stage, jobGeneration, image] { deliver(key, stage, jobGeneration, image); },
                Qt::QueuedConnection);
    };
    pool.start(run, -(stage * kStagePriorityStep + order));
}

void PagePrescaler::deliver(const PrescaleKey &key, int stage, quint64 jobGeneration, const QImage &image)
{
    const auto job = jobs.constFind(key.view.firstPage);
    if (jobGeneration != generation || job == jobs.cend() || job->key != key || job->stage != stage) {
        return;
    }
    jobs.erase(job);

    if (keep(key, image, stage)) {
        emit imageImproved(key);
    }
    startMissingStages();
}
