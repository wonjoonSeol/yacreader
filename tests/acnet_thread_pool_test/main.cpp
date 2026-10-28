#include "AC/Util/Parallel.hpp"
#include "acnet_upscale.h"

#include <QTest>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

// Anime4KCPP's loops share one thread pool, created by the first loop that needs it with the
// thread limit of that moment. So these tests have an executable to themselves, where nothing
// else uses the library first, and they run in the order declared.
class AcnetThreadPoolTest : public QObject
{
    Q_OBJECT

private slots:
    void aColourPageFirstSetsTheThreadLimit();
    void loopsRunningAtOnceShareTheThreadLimit();
    void aLoopStaysWithinTheThreadLimit();
};

namespace {

unsigned int defaultThreadLimit()
{
    return unsigned(std::max(1, QThread::idealThreadCount() / 2));
}

// The pool threads that ran the loops of `callers` threads at once, without the callers.
// Each loop asks for the whole limit, and its items take long enough for all of them to overlap.
size_t poolThreadsUsedByConcurrentLoops(int callers)
{
    std::mutex mutex;
    std::set<std::thread::id> threads, callerThreads;
    std::vector<std::thread> running;
    for (int caller = 0; caller < callers; ++caller)
        running.emplace_back([&] {
            {
                const std::lock_guard lock(mutex);
                callerThreads.insert(std::this_thread::get_id());
            }
            ac::util::parallelFor(0, 64, [&](int) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                const std::lock_guard lock(mutex);
                threads.insert(std::this_thread::get_id());
            });
        });
    for (std::thread &caller : running)
        caller.join();
    for (const std::thread::id &caller : callerThreads)
        threads.erase(caller);
    return threads.size();
}

}

// The colour path converts to YUV with the library's loops before the network runs, so the
// limit has to be in place before then, not only for the network.
void AcnetThreadPoolTest::aColourPageFirstSetsTheThreadLimit()
{
    QImage page(64, 64, QImage::Format_RGB32);
    for (int y = 0; y < page.height(); ++y)
        for (int x = 0; x < page.width(); ++x)
            page.setPixel(x, y, qRgb(4 * x, 4 * y, 128));

    QCOMPARE(upscale2xAcnet(page).size(), QSize(128, 128));
    QCOMPARE(ac::util::parallelForThreads.load(), defaultThreadLimit());
}

void AcnetThreadPoolTest::loopsRunningAtOnceShareTheThreadLimit()
{
    const size_t threads = poolThreadsUsedByConcurrentLoops(3);

    QVERIFY2(threads <= defaultThreadLimit(), qPrintable(QStringLiteral("%1 pool threads, the limit is %2").arg(threads).arg(defaultThreadLimit())));
}

void AcnetThreadPoolTest::aLoopStaysWithinTheThreadLimit()
{
    constexpr int count = 64;
    std::vector<std::atomic<int>> visits(count);
    std::mutex mutex;
    std::set<std::thread::id> threads;

    ac::util::parallelForThreads = 2;
    ac::util::parallelFor(0, count, [&](int i) {
        visits[i]++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const std::lock_guard lock(mutex);
        threads.insert(std::this_thread::get_id());
    });
    ac::util::parallelForThreads = defaultThreadLimit();

    for (int i = 0; i < count; ++i)
        QCOMPARE(visits[i].load(), 1);
    QVERIFY(threads.size() <= 2);
}

QTEST_GUILESS_MAIN(AcnetThreadPoolTest)
#include "main.moc"
