#include "proxy.hpp"
#include <vector>
#include <memory>
#include <utility>
#include <iostream>
#include <cassert>
#include <cstddef>
#include <string>
#include <unistd.h>
#include <functional>
#include <algorithm>

namespace
{

bool query_is_end(const std::vector<char>& request)
{
    if (request.size() > 4)
    {
        return ((request[request.size() - 4] == '\r'
                && request[request.size() - 3] == '\n'
                && request[request.size() - 2] == '\r'
                && request[request.size() - 1] == '\n')
                ||
                (request.size() > 7
                && request[request.size() - 1] == '>'
                && request[request.size() - 2] == 'l'
                && request[request.size() - 3] == 'm'
                && request[request.size() - 4] == 't'
                && request[request.size() - 5] == 'h'
                && request[request.size() - 6] == '/'
                && request[request.size() - 7] == '<')
                ||
                (request.size() > 9
                && request[request.size() - 1] == '\n'
                && request[request.size() - 2] == '\r'
                && request[request.size() - 3] == '>'
                && request[request.size() - 4] == 'L'
                && request[request.size() - 5] == 'M'
                && request[request.size() - 6] == 'T'
                && request[request.size() - 7] == 'H'
                && request[request.size() - 8] == '/'
                && request[request.size() - 9] == '<')
                );
    }

    return false;
}

bool is_die_events(const uint32_t events)
{
    return (events & EPOLLERR) || (events & EPOLLHUP) || (events & EPOLLRDHUP);
}

}

Proxy::Proxy(const uint16_t port, const Logger& log)
    : m_port(port)
    , m_running(false)
    , m_logger(log)
{
    m_transitions =
    {
        {ConnectionState::RECEIVING_REQUEST,       &Proxy::handle_receiving_request},
        {ConnectionState::CONNECTING_TO_SERVER,    &Proxy::handle_connecting_to_server},
        {ConnectionState::SENDING_REQUEST,         &Proxy::handle_sending_request},
        {ConnectionState::RECEIVING_RESPONSE,      &Proxy::handle_receiving_response},
        {ConnectionState::SENDING_RESPONSE,        &Proxy::handle_sending_response}
    };
}

void Proxy::start()
{
    auto code = m_server_socket.listen(m_port);
    if (code != TcpSocket::Status::DONE)
    {
        std::cerr << "error on listen\n";
        return;
    }

    std::cout << "proxy starts at " << m_port << " port" << std::endl;

    auto incoming_handler = std::bind(&Proxy::handle_incoming_connection, this, std::placeholders::_1);
    m_selector.add(m_server_socket, EPOLLIN, incoming_handler);

    m_running = true;
    while (m_running && m_selector.do_iteration());
}

void Proxy::set_port(const uint16_t port) { m_port = port; }

void Proxy::handle_receiving_request(Connection* connection)
{
    assert(connection->state == ConnectionState::RECEIVING_REQUEST);
    std::cerr << "handle_receiving_request\n";
    std::fill(m_buffer, m_buffer + m_size_of_buffer, '\0');

    std::size_t received = 0;
    auto& socket = connection->request_socket;
    assert(m_size_of_buffer > 0); // buffer size must be more than zero

    auto status = TcpSocket::Status::ERROR;
    while ((status = socket->receive(m_buffer, m_size_of_buffer - 1, &received)) == TcpSocket::Status::DONE
           && connection->state != ConnectionState::CLOSING)
    {
        handle_received_data(connection, m_buffer, received);
    }

    if (status == TcpSocket::Status::ERROR)
    {
        std::cerr << "error on receive\n";
    }
}

void Proxy::handle_connecting_to_server(Proxy::Connection* connection)
{
    std::cerr << "handle_connecting_to_server\n";

    assert(connection->state == ConnectionState::CONNECTING_TO_SERVER);
    assert(connection->response_socket != nullptr);

    auto status = TcpSocket::Status::ERROR;
    auto socket = connection->response_socket.get();
    if (connection->have_connect_called)
    {
        status = socket->isConnected();
        if (status == TcpSocket::Status::DONE)
        {
            connection->state = ConnectionState::SENDING_REQUEST;
            handle_sending_request(connection);
        }
        else if (status == TcpSocket::Status::ERROR)
        {
            std::cerr << "can't connect in handle_connecting_to_server:connect\n";
            connection->state = ConnectionState::CLOSING;
        }

        return;
    }

    status = socket->connect(IpAddress(connection->address, HTTP_PORT));
    if (status == TcpSocket::Status::DONE)
    {
        connection->state = ConnectionState::SENDING_REQUEST;
        handle_sending_request(connection);
    }
    else if (status == TcpSocket::Status::ERROR)
    {
        std::cerr << "can't connect in handle_connecting_to_server:connect\n";
        connection->state = ConnectionState::CLOSING;
    }

    connection->have_connect_called = true;
}

void Proxy::handle_sending_request(Connection* connection)
{
    std::cerr << "handle_sending_request\n";

    assert(connection->state == ConnectionState::SENDING_REQUEST);
    assert(connection->response_socket != nullptr);

    auto socket = connection->response_socket.get();
    std::size_t sent = 0;
    auto status = TcpSocket::Status::ERROR;
    while (connection->idx != connection->buffer.size())
    {
        status = socket->send(connection->buffer.data() + connection->idx, connection->buffer.size() - connection->idx, &sent);
        if (status != TcpSocket::Status::DONE)
        {
            break;
        }
        connection->idx += sent;
    }

    if (status == TcpSocket::Status::ERROR)
    {
        std::cerr << "error on handle_sending_request::send\n";
        connection->state = ConnectionState::CLOSING;
        return;
    }

    if (connection->idx == connection->buffer.size())
    {
        connection->buffer.clear();
        connection->idx = 0;
        connection->state = ConnectionState::RECEIVING_RESPONSE;
        handle_receiving_response(connection);
    }

}

void Proxy::handle_sending_response(Connection* connection)
{
    std::cerr << "handle_sending_response\n";
    assert(connection->state == ConnectionState::SENDING_RESPONSE);
    assert(connection->request_socket != nullptr);

    auto request_socket = connection->request_socket.get();
    std::size_t sent = 0;
    auto* vector = &connection->buffer;
    auto status = TcpSocket::Status::DONE;
    while (vector->size() > connection->idx && status == TcpSocket::Status::DONE)
    {
        status = request_socket->send(vector->data(), vector->size() - connection->idx, &sent);
        connection->idx += sent;
    }

    if (status == TcpSocket::Status::ERROR)
    {
        std::cerr << "error on handle_sending_response::send\n";
        connection->state = ConnectionState::CLOSING;
        return;
    }

    if (vector->size() == connection->idx)
    {
        connection->state = ConnectionState::CLOSING;
    }
}

void Proxy::handle_receiving_response(Connection* connection)
{
    std::cerr << "handle_receiving_response\n";
    assert(connection->state == ConnectionState::RECEIVING_RESPONSE);
    assert(connection->response_socket != nullptr);

    auto resposne_socket = connection->response_socket.get();
    std::size_t received = m_size_of_buffer - 1;
    auto status = TcpSocket::Status::DONE;
    while (received != 0 && status == TcpSocket::Status::DONE)
    {
        std::fill(m_buffer, m_buffer + m_size_of_buffer, 0);
        status = resposne_socket->receive(m_buffer, m_size_of_buffer - 1, &received);
        if (received != 0) {
            connection->buffer.insert(connection->buffer.end(), m_buffer, m_buffer + received);
        }

        if (query_is_end(connection->buffer))
        {
            connection->state = ConnectionState::SENDING_RESPONSE;
            handle_sending_response(connection);
            return;
        }
    }

    if (status == TcpSocket::Status::ERROR)
    {
        std::cerr << "error on handle_receiving_response::receive\n";
        connection->state = ConnectionState::CLOSING;
        return;
    }
}

void Proxy::handle_received_data(Connection* connection, char* buffer, const std::size_t received)
{
    std::cerr << "handle_received_data\n";

    buffer[received] = '\0';

    if (connection->buffer.size() > m_max_request_legnth)
    {
        // too large request, it must be an attack -> 500 Internal Server Error
        send_error(connection->request_socket.get(), "HTTP/1.0 500 OK", 16);
    }

    connection->buffer.insert(connection->buffer.end(), buffer, buffer + received);

    HttpParser::Header header = HttpParser::parse(connection->buffer);
    if (!header && connection->buffer.size() >= 4)
    {
        connection->state = ConnectionState::CLOSING;
        return;
    }

    if (query_is_end(connection->buffer) && connection->address.empty())
    {
        if (header)
        {
            if (header.method == HttpParser::Method::GET && header.version == HttpParser::Version::HTTP_1_0)
            {
                // initialize new socket and add it to the selector
                connection->address = header.URI;

                // log
                auto address = connection->request_socket->getRemoteAddress();
                auto port = connection->request_socket->getRemotePort();
                m_logger.get_stream(Logger::LOG_LEVEL::INFO)
                        << "NEW CLIENT "
                        << "Address : " << address
                        << " Port : " + std::to_string(port)
                        << " URL : " + header.URI << std::endl;

                assert(connection->response_socket == nullptr);
                connection->response_socket = std::make_unique<TcpSocket>();
                connection->state = ConnectionState::CONNECTING_TO_SERVER;
                handle_connecting_to_server(connection);

                auto handler = std::bind(&Proxy::handle_connection, this, std::placeholders::_1);
                m_selector.add(*connection->response_socket, EPOLLOUT, handler);
            }
            else
            {
                // not suppoted -> 405 Method Not Allowed
                send_error(connection->request_socket.get(), "HTTP/1.0 405 OK", 16);
                connection->state = ConnectionState::CLOSING;
            }
        }
        else
        {
            // server can't parse request -> 400 Bad Request
            send_error(connection->request_socket.get(), "HTTP/1.0 400 OK", 16);
            connection->state = ConnectionState::CLOSING;
        }
    }
}

void Proxy::send_error(TcpSocket *socket, const char* const message, const std::size_t size)
{
    std::size_t sent;
    if (socket->send(message, size, &sent) == TcpSocket::Status::ERROR) { std::cerr << "error on send_error\n"; }
    assert(sent != size);
}

void Proxy::handle_incoming_connection(const epoll_event& event)
{
    std::cerr << "handle_incoming_connection\n";

    if (is_die_events(event.events))
    {
        (event.events & EPOLLERR) ? std::cerr << "EPOLLERR\n" : std::cout << "goodby\n";
        ::close(event.data.fd);
        return;
    }

    assert(event.events & EPOLLIN);
    Connection connection(std::make_unique<TcpSocket>(-1));
    auto client_socket = connection.request_socket.get();

    auto status = TcpSocket::Status::DONE;
    while ( (status = m_server_socket.accept(client_socket) ) == TcpSocket::Status::DONE)
    {
        // Add the new connection to the selector so that we will
        // be notified when it sends something
        auto client_handler = std::bind(&Proxy::handle_connection, this, std::placeholders::_1);
        m_selector.add(*client_socket, EPOLLIN, client_handler);

        // Add the new connection to the connections list
        auto address = client_socket->getRemoteAddress();
        auto port = client_socket->getRemotePort();

        m_connections[std::pair<std::string, uint16_t>(address, port)] = std::move(connection);
    }

    if (status == TcpSocket::Status::ERROR) { std::cerr << "error on accept\n"; }

}

void Proxy::handle_connection(const epoll_event& event)
{
    auto it = m_connections.begin();
    for (; it != m_connections.end(); ++it)
    {
        if (it->second.request_socket->m_socket_fd == event.data.fd
                || it->second.response_socket->m_socket_fd == event.data.fd)
        {
            break;
        }
    }
    assert(it != m_connections.end());

    Connection* connection = &it->second;
    auto handler = m_transitions[connection->state];
    handler(this, connection);

    if (connection->state == ConnectionState::CLOSING)
    {
        std::cerr << "goodby\n";
        if (connection->request_socket)
        {
            m_selector.remove(*connection->request_socket);
        }

        if (connection->response_socket)
        {
            m_selector.remove(*connection->response_socket);
        }

        m_connections.erase(it);
    }
    else if (is_die_events(event.events))
    {
        if (!connection->response_socket && connection->response_socket->m_socket_fd == event.data.fd)
        {
            m_selector.remove(*connection->response_socket);
            connection->response_socket.reset();
            if (connection->state == ConnectionState::RECEIVING_RESPONSE && !connection->buffer.empty()) {
                connection->state = ConnectionState::SENDING_RESPONSE;
            }
        }
        else
        {
            (event.events & EPOLLERR) ? std::cerr << "EPOLLERR\n" : std::cerr << "goodby\n";
            m_connections.erase(it);
            return;
        }
    }
}
