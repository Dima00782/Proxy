#ifndef PROXY_HPP
#define PROXY_HPP

#include <SFML/Network.hpp>
#include <atomic>
#include <cstddef>
#include <utility>
#include <map>

class Proxy final
{
public:
    Proxy(const unsigned short port);

    Proxy(const Proxy&) = delete;
    Proxy& operator= (const Proxy&) = delete;

    void start();

    void set_port(const unsigned short port);

private:
    unsigned short m_port;

    std::atomic_bool m_running;

    std::size_t m_size_of_buffer;

    sf::TcpListener m_listener;

    using ID = std::pair<uint32_t, unsigned short>;
    std::map< ID, std::unique_ptr<sf::TcpSocket> > m_connections;
    sf::SocketSelector m_selector;

private:
    void handle_connections();
    void handle_incoming_connection();
};

#endif // PROXY_HPP
