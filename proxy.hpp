#ifndef PROXY_HPP
#define PROXY_HPP

#include <SFML/Network.hpp>
#include <atomic>
#include <cstddef>
#include <utility>
#include <memory>
#include <map>

#include "httpparser.hpp"

class Proxy final
{
public:
    enum class ConnectionState
    {
        RECEIVING_REQUEST,
        CONNECTING_TO_SERVER,
        SENDING_REQUEST,
        RETRANSMITTING_RESPONSE
    };

    struct Connection
    {
        Connection()
            : state(ConnectionState::RECEIVING_REQUEST)
        {}

        Connection(std::unique_ptr<sf::TcpSocket>&& _clinet_socket)
            : state(ConnectionState::RECEIVING_REQUEST)
            , request_socket(std::move(_clinet_socket))
            , idx(0)
        {}

        ConnectionState state;

        std::unique_ptr<sf::TcpSocket> request_socket;
        std::unique_ptr<sf::TcpSocket> response_socket;

        sf::IpAddress address;

        std::size_t idx;
        std::vector<char> request;
    };

public:
    Proxy(const unsigned short port);

    Proxy(const Proxy&) = delete;
    Proxy& operator= (const Proxy&) = delete;

    void start();

    void set_port(const unsigned short port);

private:
    unsigned short m_port;

    std::atomic_bool m_running;

    static const std::size_t m_size_of_buffer =  1024;

    sf::TcpListener m_listener;

    using ID = std::pair<uint32_t, unsigned short>;
    std::map<ID, Connection> m_connections;

    sf::SocketSelector m_selector;

    static const unsigned short HTTP_PORT = 80;

    char buffer[m_size_of_buffer];

private:
    void handle_incoming_connection();
    void handle_connections();

    bool handle_receiving_request(Connection *connection);
    void handle_connecting_to_server(Connection *connection);
    void handle_sending_request(Connection *connection);
    void handle_retransmitting_response(Connection *connection);

    void handle_received_data(Connection* connection, char* buffer, const std::size_t received);

    void send_error(sf::TcpSocket* socket, const char* const message, const std::size_t size);
};

#endif // PROXY_HPP
