#ifndef CONTINUOUS_PAGE_WIDGET_H
#define CONTINUOUS_PAGE_WIDGET_H

#include "acnet_upscale.h"
#include "themable.h"

#include <QHash>
#include <QImage>
#include <QList>
#include <QSize>
#include <QThreadPool>
#include <QVector>
#include <QWidget>

#include <atomic>
#include <memory>
#include <optional>

class Render;
class ContinuousViewModel;
class ContinuousPageProvider;

class ContinuousPageWidget : public QWidget, protected Themable
{
    Q_OBJECT
public:
    explicit ContinuousPageWidget(QWidget *parent = nullptr);
    ~ContinuousPageWidget() override;

    void setRender(Render *r);
    void setProvider(ContinuousPageProvider *provider);
    void setViewModel(ContinuousViewModel *viewModel);
    void reset();

    bool hasHeightForWidth() const override;
    int heightForWidth(int w) const override;
    QSize sizeHint() const override;

    // Delivers the result of a background scaling task. Runs on the GUI thread
    // (queued invocation). Public because the worker lambda is not a friend.
    void deliverScaledImage(int pageIndex, quint64 generation, QImage scaledImage,
                            qint64 sourceCacheKey, QSize sourceSize, QSize targetSize,
                            QSize targetPixelSize, qreal devicePixelRatio);
    // The same for the running ACNet task, with a null image when it did not upscale.
    void deliverUpscaledImage(QImage image);

    // The ACNet model that sharpens pages shown larger than their size, or none.
    // A change drops the scaled pages.
    void setAcnetModel(std::optional<AcnetModel> model);

public slots:
    void onPageAvailable(int absolutePageIndex);
    void invalidateScaledImageCache();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void applyTheme(const Theme &theme) override;

private:
    struct ScaledPageCacheEntry {
        qint64 sourceCacheKey = 0;
        QSize sourceSize;
        QSize targetSize;
        QSize targetPixelSize;
        qreal targetDevicePixelRatio = 1.0;
        QImage scaledImage;
        // ACNet has run on it, so it is not queued again
        bool upscaled = false;
    };

    struct ScaledPageCache {
        int effectiveWidth = -1;
        QHash<int, ScaledPageCacheEntry> pages;

        void invalidateAll()
        {
            effectiveWidth = -1;
            pages.clear();
        }

        void invalidateForWidth(int width)
        {
            if (effectiveWidth != width) {
                effectiveWidth = width;
                pages.clear();
            }
        }

        void invalidatePage(int pageIndex)
        {
            pages.remove(pageIndex);
        }

        void keepOnlyRange(int minPageIndex, int maxPageIndex)
        {
            if (pages.isEmpty()) {
                return;
            }

            QList<int> keysToRemove;
            keysToRemove.reserve(pages.size());
            for (auto it = pages.constBegin(); it != pages.constEnd(); ++it) {
                if (it.key() < minPageIndex || it.key() > maxPageIndex) {
                    keysToRemove.append(it.key());
                }
            }

            for (int key : keysToRemove) {
                pages.remove(key);
            }
        }
    };

    // Identity of an in-flight scaling request. Includes sourceCacheKey so a
    // request for re-rendered (same-size) page content is not confused with an
    // older one still in flight.
    struct ScaleRequestKey {
        qint64 sourceCacheKey = 0;
        QSize targetPixelSize;
        qreal devicePixelRatio = 1.0;
        quint64 generation = 0;

        bool operator==(const ScaleRequestKey &other) const
        {
            return sourceCacheKey == other.sourceCacheKey && targetPixelSize == other.targetPixelSize && qFuzzyCompare(devicePixelRatio, other.devicePixelRatio) && generation == other.generation;
        }
    };

    // The ACNet task in flight. It stays here until its result arrives, even once the
    // result is no longer wanted, so two never run at once.
    struct RunningUpscale {
        int pageIndex = -1;
        // the plain scale the result replaces, if it is still the page's image then
        qint64 scaledImageKey = 0;
        std::shared_ptr<std::atomic_bool> cancelled;
    };

    // Returns a cached, still-valid scaled image for the page, or nullptr.
    const QImage *cachedScaledImage(int pageIndex, const QImage *source, const QSize &targetSize, const QSize &targetPixelSize, qreal dpr) const;
    // Schedules a high-quality scale of the page on the background pool.
    void enqueueScale(int pageIndex, const QImage *source, const QSize &targetSize, const QSize &targetPixelSize, qreal dpr);
    // Starts ACNet on the next cached page that needs it, unless a task is running.
    void startNextUpscale();
    QList<int> upscaleOrder() const;
    // Drops the whole scaled cache and discards/ignores any in-flight work.
    void resetScaledCache();
    // Rect (widget coords) occupied by a page, centered horizontally.
    QRect pageRectFor(int pageIndex) const;
    // Pages in the real viewport (not the paint dirty rect, which can be a single page).
    void visiblePageRange(int &firstPageIndex, int &lastPageIndex) const;
    // Page range to keep cached / preload, derived from the real viewport
    // (not the paint dirty rect, which can be a single page).
    void managedPageRange(int &minPageIndex, int &maxPageIndex) const;

    Render *render = nullptr;
    ContinuousPageProvider *provider = nullptr;
    ContinuousViewModel *continuousViewModel = nullptr;
    ScaledPageCache scaledPageCache;

    // Bumped whenever the cache is invalidated; results from older generations
    // are discarded when they arrive from the background pool.
    quint64 cacheGeneration = 0;
    QHash<int, ScaleRequestKey> pendingScaleRequests;
    QThreadPool scalePool;

    std::optional<AcnetModel> acnetModel;
    std::optional<RunningUpscale> runningUpscale;
    // One thread, so ACNet never holds a thread the plain scales need
    QThreadPool upscalePool;
};

#endif // CONTINUOUS_PAGE_WIDGET_H
