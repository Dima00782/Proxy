#include "proxy.hpp"
#include <vector>
#include <memory>
#include <utility>
#include <iostream>
#include <cassert>
#include <cstddef>

Proxy::Proxy(unsigned short port)
    : m_port(port)
    , m_running(false)
    , m_size_of_buffer(1024)
{}

void Proxy::start()
{
    auto code = m_listener.listen(m_port);
    if (code != sf::Socket::Done)
    {
        std::cerr << "error on listen\n";
        return;
    }

    std::cout << "proxy server starts at " << m_port << " port" << std::endl;

    m_selector.add(m_listener);
    m_running = true;
    while (m_running)
    {
        // Make the selector wait for data on any socket
        if (m_selector.wait())
        {
            // Test the listener
            if (m_selector.isReady(m_listener))
            {
                handle_incoming_connection();
            }
            else
            {   // The listener socket is not ready, test all other sockets
                handle_connections();
            }
        }
    }
}

void Proxy::set_port(const unsigned short port) { m_port = port; }

void Proxy::handle_connections()
{
    char buffer[m_size_of_buffer] = {0, };
    std::size_t received = 0;

    for (auto it = m_connections.begin(); it != m_connections.end(); )
    {
        auto& connection = it->second;
        if (m_selector.isReady(*connection))
        {
            assert(m_size_of_buffer > 0); // buffer size must be more than zero
            auto status = connection->receive(buffer, m_size_of_buffer, received);
            if (status == sf::Socket::Done)
            {
                buffer[m_size_of_buffer - 1] = '\0';
                std::cout << buffer << std::endl;
                ++it;
            }
            else
            {
                m_selector.remove(*connection);
                it = m_connections.erase(it);

                if (status != sf::Socket::Disconnected)
                {
                    std::cerr << "error on receive\n";
                }
                else
                {
                    std::cout << "goodby" << std::endl;
                }
            }
        }
    }
}

void Proxy::handle_incoming_connection()
{
    // The listener is ready: there is a pending connection
    auto connection = std::make_unique<sf::TcpSocket>();
    auto status = m_listener.accept(*connection);
    if (status == sf::Socket::Done)
    {
        connection->setBlocking(false);

        // Add the new connection to the selector so that we will
        // be notified when he sends something
        m_selector.add(*connection);

        // Add the new connection to the connections list
        auto address = connection->getRemoteAddress().toInteger();
        auto port = connection->getRemotePort();
        m_connections[std::pair<uint32_t, unsigned short>(address, port)] = std::move(connection);
    }
    else if (status == sf::Socket::NotReady)
    {
        std::cout << "not ready" << std::endl;
    }
    else
    {
        std::cerr << "error on accept\n";
    }
}
