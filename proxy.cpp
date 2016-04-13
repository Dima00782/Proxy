#include "proxy.hpp"
#include <vector>
#include <memory>
#include <utility>
#include <iostream>
#include <cassert>
#include <cstddef>
#include <string>

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

Proxy::Proxy(unsigned short port, const Logger& log)
    : m_port(port)
    , m_running(false)
    , m_logger(log)
{
    transitions =
    {
        { ConnectionState::RECEIVING_REQUEST,       &Proxy::handle_receiving_request},
        { ConnectionState::CONNECTING_TO_SERVER,    &Proxy::handle_connecting_to_server},
        { ConnectionState::SENDING_REQUEST,         &Proxy::handle_sending_request},
        { ConnectionState::RETRANSMITTING_RESPONSE, &Proxy::handle_retransmitting_response}
    };
}

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
        Connection* connection = &it->second;
        auto handler = transitions[connection->state];
        if (handler(this, connection)) { it = m_connections.erase(it); }
        else { ++it; }
    }
}

bool Proxy::handle_receiving_request(Connection* connection)
{
    std::size_t received = 0;
    auto& socket = connection->request_socket;

    if (m_selector.isReady(*socket))
    {
        assert(m_size_of_buffer > 0); // buffer size must be more than zero
        auto status = socket->receive(buffer, m_size_of_buffer - 1, received);
        if (status == sf::Socket::Done || status == sf::Socket::NotReady)
        {
            return handle_received_data(connection, buffer, received);
        }

        m_selector.remove(*socket);
        if (status != sf::Socket::Disconnected)
        {
            std::cerr << "error on receive\n";
        }
        else
        {
            std::cout << "goodby\n";
        }
        return true;
    }

    return false;
}

bool Proxy::handle_connecting_to_server(Proxy::Connection* connection)
{
    assert(connection->state == ConnectionState::CONNECTING_TO_SERVER);
    assert(connection->response_socket != nullptr);

    auto socket = connection->response_socket.get();
    auto status = socket->connect(connection->address, HTTP_PORT);
    if (status == sf::Socket::NotReady)
    {
        // nothing to do here
    }
    else if (status == sf::Socket::Done)
    {
        std::size_t sent = 0;
        auto& vector = connection->request;
        auto status = connection->response_socket->send(vector.data() + connection->idx, vector.size() - connection->idx, sent);
        if (status == sf::Socket::Error)
        {
            std::cerr << "error on handle_connecting_to_server:send\n";
            m_selector.remove(*socket);
            m_selector.remove(*connection->request_socket);
            return true;
        }
        connection->idx += sent;
        connection->state = ConnectionState::SENDING_REQUEST;
    }
    else
    {
        std::cerr << "can't connect in handle_connecting_to_server:connect\n";
        m_selector.remove(*socket);
        m_selector.remove(*connection->request_socket);
        return true;
    }

    return false;
}

bool Proxy::handle_sending_request(Connection* connection)
{
    assert(connection->state == ConnectionState::SENDING_REQUEST);
    assert(connection->response_socket != nullptr);

    auto socket = connection->response_socket.get();
    if (m_selector.isReady(*socket))
    {
        if (connection->idx != connection->request.size())
        {
            std::size_t sent = 0;
            auto status = socket->send(connection->request.data() + connection->idx, connection->request.size() - connection->idx, sent);
            if (status == sf::Socket::Error)
            {
                std::cerr << "error on handle_sending_request::send\n";
                return true;
            }
            connection->idx += sent;
        }
        else
        {
            connection->request.clear();
            connection->idx = 0;
            connection->state = ConnectionState::RETRANSMITTING_RESPONSE;
        }
    }

    return false;
}

bool Proxy::handle_retransmitting_response(Connection* connection)
{
    assert(connection->state == ConnectionState::RETRANSMITTING_RESPONSE);
    assert(connection->request_socket != nullptr);
    assert(connection->response_socket != nullptr);

    auto resposne_socket = connection->response_socket.get();
    auto request_socket = connection->request_socket.get();
    if (m_selector.isReady(*resposne_socket))
    {
        std::size_t received;
        auto status = resposne_socket->receive(buffer, m_size_of_buffer - 1, received);
        if (status == sf::Socket::NotReady)
        {
            connection->request.insert(connection->request.end(), buffer, buffer + received);
        }
        else if (status == sf::Socket::Error)
        {
            std::cerr << "error on handle_retransmitting_response::receive\n";
            m_selector.remove(*request_socket);
            m_selector.remove(*resposne_socket);
            return true;
        }
    }

    if (m_selector.isReady(*request_socket))
    {
        std::size_t sent;
        auto* vector = &connection->request;
        if (vector->size() < connection->idx)
        {
            auto status = request_socket->send(vector->data(), vector->size() - connection->idx, sent);
            if (status == sf::Socket::Done || status == sf::Socket::Error)
            {
                if (status == sf::Socket::Error) { std::cerr << "error on handle_retransmitting_response::send\n"; }

                m_selector.remove(*request_socket);
                m_selector.remove(*resposne_socket);
                return true;
            }
            else { connection->idx += sent; }
        }
        else
        {
            m_selector.remove(*request_socket);
            m_selector.remove(*resposne_socket);
            return true;

        }
    }

    return false;
}

bool Proxy::handle_received_data(Connection* connection, char* buffer, const std::size_t received)
{
    buffer[received] = '\0';

    if (connection->request.size() > m_max_request_legnth)
    {
        // too large request, it must be an attack -> 500 Internal Server Error
        send_error(connection->request_socket.get(), "HTTP/1.0 500 OK", 16);
    }

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

                // log
                auto address = connection->request_socket->getRemoteAddress().toInteger();
                auto port = connection->request_socket->getRemotePort();
                m_logger.get_stream(Logger::LOG_LEVEL::INFO)
                        << "NEW CLIENT "
                        << "Address : " << std::to_string(address)
                        << " Port : " + std::to_string(port)
                        << " URL : " + header.URI << std::endl;

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
                m_selector.remove(*connection->request_socket);
                return true;
            }
        }
        else
        {
            // server can't parse request -> 400 Bad Request
            send_error(connection->request_socket.get(), "HTTP/1.0 400 OK", 16);
            m_selector.remove(*connection->request_socket);
            return true;
        }
    }

    return false;
}

void Proxy::send_error(sf::TcpSocket *socket, const char* const message, const std::size_t size)
{
    std::size_t sent;
    if (socket->send(message, size, sent) != sf::Socket::Done) { std::cerr << "error on send_error\n"; }
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
    else if (status == sf::Socket::NotReady) { std::cerr << "not ready\n"; }
    else { std::cerr << "error on accept\n"; }
}
