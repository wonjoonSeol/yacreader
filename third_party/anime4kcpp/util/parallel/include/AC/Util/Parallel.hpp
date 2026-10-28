#ifndef AC_UTIL_PARALLEL_HPP
#define AC_UTIL_PARALLEL_HPP

#if defined(AC_DEP_PARALLEL_PPL)
#   include <ppl.h>
#elif defined(AC_DEP_PARALLEL_OPENMP)
#else
#   include <algorithm>
#   include <atomic>
#   include "AC/Util/ThreadPool.hpp"
#endif

namespace ac::util
{
    template <typename IndexType, typename F>
    void parallelFor(IndexType first, IndexType last, F&& func);

#if !defined(AC_DEP_PARALLEL_PPL) && !defined(AC_DEP_PARALLEL_OPENMP)
    // YACReader: the most threads a parallelFor call uses, 0 for all hardware threads. The pool takes its size from
    // it on first use, so it also caps the threads of calls that run at the same time.
    inline std::atomic<unsigned int> parallelForThreads{ 0 };

    namespace detail
    {
        inline unsigned int parallelForThreadLimit() noexcept
        {
            const unsigned int hardwareThreads = ThreadPool::hardwareThreads();
            const unsigned int limit = parallelForThreads.load(std::memory_order_relaxed);
            return limit ? std::min(limit, hardwareThreads) : hardwareThreads;
        }

        // YACReader: one pool shared by every parallelFor instantiation instead of one each.
        inline ThreadPool& parallelForPool()
        {
            static ThreadPool pool{ parallelForThreadLimit() };
            return pool;
        }
    }
#endif
}

template <typename IndexType, typename F>
inline void ac::util::parallelFor(const IndexType first, const IndexType last, F&& func)
{
#if defined(AC_DEP_PARALLEL_PPL)
    Concurrency::parallel_for(first, last, std::forward<F>(func), Concurrency::static_partitioner());
#elif defined(AC_DEP_PARALLEL_OPENMP)
#   pragma omp parallel for schedule(guided)
    for (IndexType i = first; i < last; i++) func(i);
#else
    const auto threads = static_cast<IndexType>(detail::parallelForThreadLimit());
    const IndexType range = last - first;
    const IndexType tasks = std::min(threads, range);
    if (tasks > 1)
    {
        auto& pool = detail::parallelForPool();
        std::vector<std::future<void>> futures{};
        futures.reserve(static_cast<std::size_t>(tasks));
        for (IndexType t = 0; t < tasks; t++)
        {
            const IndexType begin = first + range * t / tasks;
            const IndexType end = first + range * (t + 1) / tasks;
            futures.emplace_back(pool.exec([&func](const IndexType startIdx, const IndexType endIdx) { for (IndexType idx = startIdx; idx < endIdx; idx++) func(idx); }, begin, end));
        }
        for (auto&& future : futures) future.wait();
    }
    else for (IndexType i = first; i < last; i++) func(i);
#endif
}

#endif
