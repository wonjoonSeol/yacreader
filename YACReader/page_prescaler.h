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
    ScaleMethod scaleMethod = ScaleMethod::Lanczos4;

    bool operator==(const PrescaleKey &other) const = default;
};

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
    // Called on pool threads, for several views at once
    virtual QImage process(const QImage &image, QSize pixelSize, ScaleMethod scaleMethod) const = 0;
};

// The same scale the viewer does itself when a page is not prescaled yet
class ScaleStage : public PrescaleStage
{
public:
    QImage process(const QImage &image, QSize pixelSize, ScaleMethod scaleMethod) const override;
};

// Prepares the images of the views around the current one in the background, so page turns do not have to scale
// on the GUI thread. Only the views of the last schedule() are kept.
class PagePrescaler : public QObject
{
    Q_OBJECT
public:
    explicit PagePrescaler(QObject *parent = nullptr);
    PagePrescaler(QList<std::shared_ptr<const PrescaleStage>> stages, int maxThreads, QObject *parent = nullptr);
    ~PagePrescaler() override;

    // The best image so far for the key, or a null image
    QImage image(const PrescaleKey &key) const;
    // Keeps an image scaled by the caller as the first stage's result
    void store(const PrescaleKey &key, const QImage &image);
    // Keeps only these views and runs the stages they are missing: every view's first stage before any later one,
    // and the views in the order given
    void schedule(const QList<PrescaleRequest> &requests);
    // Drops the images and the work in flight, for changes the keys do not show
    void invalidate();

signals:
    void imageImproved(const PrescaleKey &key);

private:
    struct Entry {
        PrescaleKey key;
        QImage image;
        int stage = -1;
    };

    struct JobState {
        std::atomic_bool started = false;
        std::atomic_bool cancelled = false;
    };

    struct Job {
        PrescaleKey key;
        int stage = 0;
        std::shared_ptr<JobState> state;
    };

    bool isRequested(const PrescaleKey &key) const;
    bool keep(const PrescaleKey &key, const QImage &image, int stage);
    void startMissingStages();
    void start(const PrescaleRequest &request, int stage, int order);
    void deliver(const PrescaleKey &key, int stage, quint64 jobGeneration, const QImage &image);

    QList<std::shared_ptr<const PrescaleStage>> stages;
    QList<PrescaleRequest> requests;
    // by first page
    QHash<int, Entry> entries;
    // by first page, one stage at a time per view
    QHash<int, Job> jobs;
    quint64 generation = 0;
    QThreadPool pool;
};

#endif // PAGE_PRESCALER_H
