#include "proxy.hpp"
#include <vector>
#include <memory>
#include <utility>
#include <iostream>
#include <cassert>
#include <cstddef>

namespace
{

bool request_is_end(const std::vector<char>& request)
{
    if (request.size() > 4)
    {

        return request[request.size() - 4] == '\r'
                && request[request.size() - 3] == '\n'
                && request[request.size() - 2] == '\r'
                && request[request.size() - 1] == '\n';
    }

    return false;
}

}

Proxy::Proxy(unsigned short port)
    : m_port(port)
    , m_running(false)
{}

void Proxy::start()
{
    auto code = m_listener.listen(m_port);
    if (code != sf::Socket::Done)
    {
        std::cerr << "error on listen\n";
        return;
    }

    std::cout << "proxy starts at " << m_port << " port" << std::endl;

    m_listener.setBlocking(false);
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
            {
                handle_connections();
            }
        }
    }
}

void Proxy::set_port(const unsigned short port) { m_port = port; }

void Proxy::handle_connections()
{
    for (auto it = m_connections.begin(); it != m_connections.end(); )
    {
        Connection& connection = it->second;
        if (connection.state == ConnectionState::RECEIVING_REQUEST)
        {
            it = handle_receiving_request(&connection) ? ++it : m_connections.erase(it);
        }
        else if (connection.state == ConnectionState::CONNECTING_TO_SERVER)
        {
            handle_connecting_to_server(&connection);
            ++it;
        }
        else if (connection.state == ConnectionState::SENDING_REQUEST)
        {
            handle_sending_request(&connection);
            ++it;
        }
        else if (connection.state == ConnectionState::RETRANSMITTING_RESPONSE)
        {
            handle_retransmitting_response(&connection);
            ++it;
        }
    }
}

bool Proxy::handle_receiving_request(Connection* connection)
{
    std::size_t received = 0;
    auto& socket = connection->request_socket;

    if (m_selector.isReady(*socket))
    {
        assert(m_size_of_buffer > 0); // buffer size must be more than zero
        auto status = socket->receive(buffer, m_size_of_buffer, received);
        if (status == sf::Socket::Done || status == sf::Socket::NotReady)
        {
            handle_received_data(connection, buffer, received);
            return true;
        }

        m_selector.remove(*socket);
        if (status != sf::Socket::Disconnected)
        {
            std::cerr << "error on receive\n";
        }
        else
        {
            std::cout << "goodby" << std::endl;
        }
    }

    return false;
}

void Proxy::handle_connecting_to_server(Proxy::Connection* connection)
{
    assert(connection->state == ConnectionState::CONNECTING_TO_SERVER);
    assert(connection->response_socket != nullptr);

    auto& socket = *connection->response_socket;
    auto status = socket.connect(connection->address, HTTP_PORT, sf::milliseconds(10));
    if (status == sf::Socket::NotReady)
    {
        // nothing to do here
    }
    else if (status == sf::Socket::Done)
    {
        std::size_t sent = 0;
        auto& vector = connection->request;
        connection->response_socket->send(vector.data() + connection->idx, vector.size() - connection->idx, sent);
        connection->idx += sent;
        connection->state = ConnectionState::SENDING_REQUEST;
    }
}

void Proxy::handle_sending_request(Connection* connection)
{
    assert(connection->state == ConnectionState::SENDING_REQUEST);
    assert(connection->response_socket != nullptr);

    auto& socket = *connection->response_socket;
    if (m_selector.isReady(socket))
    {
        if (connection->idx != connection->request.size())
        {
            std::size_t sent = 0;
            socket.send(connection->request.data() + connection->idx, connection->request.size() - connection->idx, sent);
            connection->idx += sent;
        }
        else
        {
            connection->state = ConnectionState::RETRANSMITTING_RESPONSE;
        }
    }
}

void Proxy::handle_retransmitting_response(Connection* connection)
{
    assert(connection->state == ConnectionState::RETRANSMITTING_RESPONSE);
    assert(connection->request_socket != nullptr);
    assert(connection->response_socket != nullptr);

    auto socket = connection->response_socket.get();
    if (m_selector.isReady(socket))
    {
    }
}

void Proxy::handle_received_data(Connection* connection, char* buffer, const std::size_t received)
{
    buffer[received] = '\0';

    // add checks for very large request?
    connection->request.insert(connection->request.end(), buffer, buffer + received);

    if (request_is_end(connection->request))
    {
        HttpParser::Header header = HttpParser::parse(connection->request);
        if (header)
        {
            if (header.method == HttpParser::Method::GET && header.version == HttpParser::Version::HTTP_1_0)
            {
                // initialize new socket and add it to the selector
                connection->address = sf::IpAddress(header.URI);
                assert(connection->response_socket == nullptr);
                connection->response_socket = std::make_unique<sf::TcpSocket>();

                auto socket = connection->response_socket.get();
                socket->setBlocking(false);
                auto status = socket->connect(connection->address, HTTP_PORT);
                if (status == sf::Socket::Done)
                {
                    connection->state = ConnectionState::SENDING_REQUEST;
                }
                connection->state = ConnectionState::CONNECTING_TO_SERVER;
                m_selector.add(*socket);
            }
            else
            {
                // not suppoted -> 405 Method Not Allowed
                send_error(connection->request_socket.get(), "HTTP/1.0 405 OK", 16);

                // delete from map
                m_selector.remove(*connection->request_socket);
            }
        }
        else
        {
            // server can't parse request -> 400 Bad Request
            send_error(connection->request_socket.get(), "HTTP/1.0 400 OK", 16);

            // delete from map
            m_selector.remove(*connection->request_socket);
        }

        // log
        for (auto ch : connection->request) { std::cout << ch; }
        std::cout << std::endl;
    }
}

void Proxy::send_error(sf::TcpSocket *socket, const char* const message, const std::size_t size)
{
    std::size_t sent;
    if (socket->send(message, size, sent) != sf::Socket::Done)
    {
        std::cerr << "error on send_error" << std::endl;
    }

    assert(sent != size);
}

void Proxy::handle_incoming_connection()
{
    // The listener is ready: there is a pending connection
    auto socket = std::make_unique<sf::TcpSocket>();
    Connection connection(std::move(socket));
    auto client_socket = connection.request_socket.get();
    auto status = m_listener.accept(*client_socket);
    if (status == sf::Socket::Done)
    {
        client_socket->setBlocking(false);

        // Add the new connection to the selector so that we will
        // be notified when he sends something
        m_selector.add(*client_socket);

        // Add the new connection to the connections list
        auto address = client_socket->getRemoteAddress().toInteger();
        auto port = client_socket->getRemotePort();
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
