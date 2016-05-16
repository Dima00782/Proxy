#ifndef PROXY_HPP
#define PROXY_HPP

#include <atomic>
#include <cstddef>
#include <utility>
#include <memory>
#include <map>
#include <functional>
#include <string>

#include "tcpsocket.hpp"
#include "selector.hpp"
#include "httpparser.hpp"
#include "logger.hpp"
#include "scheduler.hpp"

class Proxy final
{
public:
    enum class ConnectionState
    {
        RECEIVING_REQUEST,
        CONNECTING_TO_SERVER,
        SENDING_REQUEST,
        RECEIVING_RESPONSE,
        SENDING_RESPONSE,
        SENDING_ERROR,
        CLOSING
    };

    struct Connection
    {
        Connection()
            : state(ConnectionState::RECEIVING_REQUEST)
            , have_connect_called(false)
        {}

        Connection(std::unique_ptr<TcpSocket>&& _clinet_socket)
            : state(ConnectionState::RECEIVING_REQUEST)
            , request_socket(std::move(_clinet_socket))
            , idx(0)
            , have_connect_called(false)
        {}

        ConnectionState state;

        std::unique_ptr<TcpSocket> request_socket;
        std::unique_ptr<TcpSocket> response_socket;

        std::string address;
        std::size_t idx;
        std::string buffer;

        bool have_connect_called;
    };

public:
    Proxy(const uint16_t port, const Logger& log, std::unique_ptr<Scheduler>&& scheduler);

    Proxy(const Proxy&) = delete;
    Proxy& operator= (const Proxy&) = delete;

    void start();

    void set_port(const uint16_t port);

private:
    uint16_t m_port;

    std::atomic_bool m_running;

    static const std::size_t m_size_of_buffer =  1024;

    static const std::size_t m_max_request_legnth = 2048;

    TcpSocket m_server_socket;

    using ID = std::pair<std::string, uint16_t>;

    std::map<ID, Connection> m_connections;

    Selector m_selector;

    char m_buffer[m_size_of_buffer];

    std::map < ConnectionState, std::function<void(Proxy*, Connection*)> > m_transitions;

    Logger m_logger;

    static const uint16_t HTTP_PORT = 80;

    std::unique_ptr<Scheduler> m_scheduler;

private:
    void handle_incoming_connection(const epoll_event &event);
    void handle_connection(const epoll_event& event);

    void handle_connections();

    void handle_receiving_request(Connection *connection);
    void handle_connecting_to_server(Connection *connection);
    void handle_sending_request(Connection *connection);
    void handle_receiving_response(Connection* connection);
    void handle_sending_response(Connection* connection);
    void handle_sending_error(Connection* connection);

    void handle_received_data(Connection* connection, char* m_buffer, const std::size_t received);

    void send_error(Connection* socket, const std::string& message);
};

#endif // PROXY_HPP
