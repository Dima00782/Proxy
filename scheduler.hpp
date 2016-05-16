#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "functionwrapper.hpp"
#include <future>
#include <type_traits>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <queue>
#include <functional>
#include <thread>

class Scheduler
{
public:
    Scheduler(std::size_t number_of_threads = 0);

    virtual ~Scheduler();

    template<class TFunction, class... Args>
    auto schedule(TFunction&& function, Args&&... args) -> std::future<typename std::result_of<TFunction(Args...)>::type>;

private:
    std::condition_variable m_condition;

    std::mutex m_queue_mutex;

    bool m_stop;

    std::vector<std::thread> m_threads;

    std::queue<FunctionWrapper> m_tasks;
};

template<class TFunction, class... Args>
auto Scheduler::schedule(TFunction&& function, Args&&... args) -> std::future< typename std::result_of<TFunction(Args...)>::type >
{
    std::lock_guard<std::mutex> lock(m_queue_mutex);

    // don't allow enqueueing after stopping the pool
    if (m_stop)
    {
        throw std::runtime_error("schedule on destroyed scheduler");
    }

    typedef typename std::result_of<TFunction(Args...)>::type result_type;

    std::packaged_task<result_type()> task(std::bind(std::forward<TFunction>(function), std::forward<Args>(args)...));

    std::future<result_type> result(task.get_future());
    m_tasks.push(std::move(task));
    m_condition.notify_one();

    return result;
}

#endif // SCHEDULER_HPP
