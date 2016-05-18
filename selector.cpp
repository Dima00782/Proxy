#include "selector.hpp"
#include <iostream>
#include <utility>
#include <cassert>

Selector::Selector()
    : m_size(0)
{
    m_selector_fd = epoll_create1(0);
    if (m_selector_fd == -1)
    {
        std::cerr << "error in epoll_create\n";
        return;
    }
}

void Selector::add(const TcpSocket& socket, const uint32_t mode, const THandler& handler)
{
    auto event = std::make_unique<epoll_event>();
    event->data.fd = socket.m_socket_fd;
    event->events = mode;
    event->events |= EPOLLET; // always add edge-triggered mode
    int return_code = epoll_ctl(m_selector_fd, EPOLL_CTL_ADD, socket.m_socket_fd, event.get());
    if (return_code == -1)
    {
        perror("epoll_ctl:add");
        return;
    }
    m_events[socket.m_socket_fd] = Event(std::move(event), handler);
    ++m_size;
    m_buffer.reserve(m_size);
}

void Selector::remove(const TcpSocket& socket)
{
    auto event_iterator = m_events.find(socket.m_socket_fd);
    assert(event_iterator != m_events.end()); // you trying to delete socket that isn't in selector

    auto event = event_iterator->second.m_event_ptr.get();
    int return_code = epoll_ctl(m_selector_fd, EPOLL_CTL_DEL, socket.m_socket_fd, event);
    if (return_code == -1)
    {
        perror("epoll_ctl:remove");
        return;
    }
    m_events.erase(event_iterator);
    --m_size;
}

void Selector::change_mode(const TcpSocket& socket, const uint32_t mode)
{
    auto event_iterator = m_events.find(socket.m_socket_fd);
    assert(event_iterator != m_events.end()); // you trying to change socket that isn't in selector

    auto event = event_iterator->second.m_event_ptr.get();
    event->events = mode;
    event->events |= EPOLLET; // always add edge-triggered mode
    int return_code = epoll_ctl(m_selector_fd, EPOLL_CTL_MOD, socket.m_socket_fd, event);
    if (return_code == -1)
    {
        perror("epoll_ctl:change_mode");
        return;
    }
}

namespace
{

const auto default_handler = [](Selector*, const epoll_event&)
{
    std::cerr << "default handler\n";
    /* nothing to do here */
};

}

bool Selector::do_iteration()
{
    std::cout << "epoll wait " << m_size << std::endl;
    int n = ::epoll_wait(m_selector_fd, m_buffer.data(), m_size, -1);
    if (n >= 0)
    {
        auto events = m_buffer.data();
        for (int i = 0; i < n; ++i)
        {
            auto event_iterator = m_events.find(events[i].data.fd);
            assert(event_iterator != m_events.end()); // something wrong with add
            Event& event = event_iterator->second;
            event.m_handler(events[i]);
        }

        return true;
    }

    return false;
}

Selector::Event::Event(std::unique_ptr<epoll_event>&& event_ptr, const THandler& handler)
    : m_event_ptr(std::move(event_ptr))
    , m_handler(handler)
{}
