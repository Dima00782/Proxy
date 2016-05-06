#ifndef TCP_SOCKET_HPP
#define TCP_SOCKET_HPP

#include "ipaddress.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

class TcpSocket
{
public:
    enum class Status
    {
        DONE,
        NOT_READY,
        ERROR
    };

public:
    TcpSocket();
    TcpSocket(int file_descriptor);
    virtual ~TcpSocket();

    Status connect(const IpAddress& remoteAddress);
    Status isConnected() const;

    Status listen();
    Status listen(const uint16_t port);

    Status bind(const IpAddress& remoteAddress);
    Status bind(const uint16_t port);

    Status accept(TcpSocket* client);

    Status send(const char *data, const std::size_t size, std::size_t* sent);
    Status receive(char *data, const std::size_t size, std::size_t* received);

    uint16_t getRemotePort() const;
    std::string getRemoteAddress() const;

private:
    int m_socket_fd;

    bool m_is_bound;

    uint16_t m_remote_port;

    std::string m_remote_host;

    friend class Selector;

    // TODO
    friend class Proxy;
};

#endif // TCP_SOCKET_HPP
