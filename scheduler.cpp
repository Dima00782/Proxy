#include "scheduler.hpp"
#include <utility>
#include <thread>
#include <iostream>

Scheduler::Scheduler(std::size_t number_of_threads)
    : m_stop(false)
{
    if (0 == number_of_threads)
    {
        number_of_threads = std::thread::hardware_concurrency();
        if (number_of_threads == 0) {
            number_of_threads = 2;
        }
    }

    for (std::size_t i = 0; i < number_of_threads; ++i)
    {
        m_threads.emplace_back([this]
        {
            while (true)
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                while(!m_stop && m_tasks.empty())
                {
                    m_condition.wait(lock);
                }

                if (m_stop && m_tasks.empty())
                {
                    return;
                }

                FunctionWrapper task = std::move(m_tasks.front());
                m_tasks.pop();
                lock.unlock();
                task();
            }
        });
    }
}

Scheduler::~Scheduler()
{
    {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        m_stop = true;
    }

    m_condition.notify_all();

    for (std::size_t i = 0; i < m_threads.size(); ++i)
    {
        m_threads[i].join();
    }
}
