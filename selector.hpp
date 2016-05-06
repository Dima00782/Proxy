#ifndef SELECTOR_HPP
#define SELECTOR_HPP

#include "tcpsocket.hpp"
#include <sys/epoll.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

class Selector final
{
public:
    using THandler = std::function<void(const epoll_event& event)>;

public:
    Selector();

    void add(const TcpSocket& socket, const uint32_t mode, const THandler& handler);
    void remove(const TcpSocket& socket);
    void change_mode(const TcpSocket& socket, const uint32_t mode);
    bool do_iteration();

private:
    struct Event
    {
        Event() = default;
        Event(std::unique_ptr<epoll_event>&& event_ptr, const THandler& handler);

        std::unique_ptr<epoll_event> m_event_ptr;
        THandler m_handler;
    };

private:
    int m_selector_fd;
    std::size_t m_size;

    std::unordered_map< int, Event > m_events;

    std::vector<epoll_event> m_buffer;
};

#endif // SELECTOR_HPP
