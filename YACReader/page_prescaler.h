#ifndef PAGE_PRESCALER_H
#define PAGE_PRESCALER_H

#include "page_view.h"
#include "resize_image.h"

#include <QHash>
#include <QImage>
#include <QList>
#include <QObject>
#include <QSize>
#include <QThreadPool>

#include <atomic>
#include <memory>

// Identifies the image shown for a page view at one size
struct PrescaleKey {
    PageViewKey view;
    QSize pixelSize;
    qreal devicePixelRatio = 1.0;
    ScaleMethod scaleMethod = ScaleMethod::Lanczos;

    bool operator==(const PrescaleKey &other) const = default;
};

inline size_t qHash(const PrescaleKey &key, size_t seed = 0)
{
    return qHashMulti(seed, key.view, key.pixelSize.width(), key.pixelSize.height(), key.devicePixelRatio, static_cast<int>(key.scaleMethod));
}

struct PrescaleRequest {
    PrescaleKey key;
    PageViewImages images;
};

// One step of the prescale pipeline. The stages of a view run one after the other, each one giving a better image
// for the same key than the one before (an upscaler after the plain scale, for example), so a page is shown as soon
// as the first stage is done and replaced when a later one is.
class PrescaleStage
{
public:
    virtual ~PrescaleStage() = default;

    // Called on the GUI thread before the stage is queued for a view, so a stage with nothing to add to it declines
    // before a double page is drawn for it. imageSize is the size of the image process() would get.
    virtual bool isActive(QSize imageSize, QSize pixelSize) const;
    // The most views the stage works on at once. A slow stage that takes fewer than the prescaler's threads leaves
    // the others to the earlier stages.
    virtual int maxConcurrent() const;
    // Called on pool threads. image is the view at full resolution, a double page already drawn together; returns it
    // at pixelSize, or a null image to keep the previous stage's one. cancelled is set once the result is no longer
    // wanted (the view left the window, or the prescaler was invalidated or destroyed), so a slow stage can stop
    // early; what it returns then is dropped.
    virtual QImage process(const QImage &image, QSize pixelSize, ScaleMethod scaleMethod, const std::atomic_bool &cancelled) const = 0;
};

// The plain scale, with the scaling method of the options
class ScaleStage : public PrescaleStage
{
public:
    QImage process(const QImage &image, QSize pixelSize, ScaleMethod scaleMethod, const std::atomic_bool &cancelled) const override;
};

// Prepares the images of the views around the current one in the background, so page turns do not have to scale
// on the GUI thread
class PagePrescaler : public QObject
{
    Q_OBJECT
public:
    // The scaled images kept, at 4 bytes a pixel: four double pages fitted to a 4K screen (2880 x 2160 px) or four
    // single pages fitted to the width of a 1440p one (2560 x 3840 px) fit, a page zoomed in a lot keeps fewer views
    static constexpr qsizetype defaultMaxBytes = qsizetype(256) * 1024 * 1024;
    // Render decodes one page at a time, so a couple of threads next to it keep most cores free
    static int defaultThreadCount();

    // The first stage is also the one scaleNow() runs. The views of a schedule() are kept in its order while their
    // images fit in maxBytes, and the first one always is.
    explicit PagePrescaler(QList<std::shared_ptr<const PrescaleStage>> stages, int maxThreads = defaultThreadCount(),
                           qsizetype maxBytes = defaultMaxBytes, QObject *parent = nullptr);
    ~PagePrescaler() override;

    // The best image so far for the key, or a null image
    QImage image(const PrescaleKey &key) const;
    // The best image so far, or else the first stage's one made on the calling thread, for a view that has to be
    // shown now. The view's first stage in the background is cancelled, and the image is kept like a background one;
    // a view the last schedule() left out is kept until the next schedule() or scaleNow() of another view.
    QImage scaleNow(const PrescaleRequest &request);
    // Keeps only the views of these requests that fit in the byte budget and runs the stages they are missing: every
    // view's first stage before any later one, and the views in the order given. Work for the views left out is
    // cancelled, and work that has not started yet is queued again in the new order.
    void schedule(const QList<PrescaleRequest> &requests);
    // Drops the images and the work in flight, for changes the keys do not show
    void invalidate();

signals:
    void imageImproved(const PrescaleKey &key);

private:
    struct Source;

    struct Job {
        int stage = 0;
        // set by whoever takes the job first: a pool thread that runs it, or schedule() and invalidate() taking it
        // back before it starts
        std::atomic_bool claimed = false;
        std::atomic_bool cancelled = false;
    };

    struct View {
        QImage image;
        // the last stage that ran
        int stage = -1;
        // the image the stages start from, kept while the view has stages left to run
        std::shared_ptr<Source> source;
        // the stage queued or running
        std::shared_ptr<Job> job;
    };

    bool isRequested(const PrescaleKey &key) const;
    int nextStage(const PrescaleRequest &request, int after) const;
    void startMissingStages();
    void start(const PrescaleKey &key, View &view, int stage, int order);
    bool takeBack(View &view);
    void cancel(View &view);
    void finish(const PrescaleKey &key, const std::shared_ptr<Job> &job, const QImage &image);

    QList<std::shared_ptr<const PrescaleStage>> stages;
    qsizetype maxBytes;
    QList<PrescaleRequest> requests;
    QHash<PrescaleKey, View> views;
    // jobs of each stage that are queued or running, including cancelled ones that are still running
    QList<int> busy;
    QThreadPool pool;
};

#endif // PAGE_PRESCALER_H
