#include "tcpsocket.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <cassert>
#include <cerrno>
#include <cstdio>

TcpSocket::TcpSocket() : TcpSocket(-1)
{
    // all socket are in nonblock state by default
    int fd = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if (fd == -1)
    {
        perror("socket");
        return;
    }

    m_socket_fd = fd;
}

TcpSocket::TcpSocket(int file_descriptor)
    : m_socket_fd(file_descriptor)
    , m_is_bound(false)
    , m_remote_port(0)
{}

TcpSocket::~TcpSocket()
{
    if (m_socket_fd != -1)
    {
        ::close(m_socket_fd);
    }
}

TcpSocket::Status TcpSocket::connect(const IpAddress& remoteAddress)
{
    assert(m_socket_fd != -1);
    addrinfo* address = remoteAddress.get_address_info();
    int result_code = ::connect(m_socket_fd, address->ai_addr, address->ai_addrlen);
    if (result_code == -1)
    {
        if (errno == EINPROGRESS)
        {
            return Status::NOT_READY;
        }
        else
        {
            perror("connect");
            return Status::ERROR;
        }
    }

    return Status::DONE;
}

TcpSocket::Status TcpSocket::isConnected() const
{
    int optval = 0;
    socklen_t optval_length = sizeof(int);

    int return_code = ::getsockopt(m_socket_fd, SOL_SOCKET, SO_ERROR, &optval, &optval_length);
    if (return_code != 0)
    {
        perror("getsockopt:isConnected");
        return Status::ERROR;
    }

    if (optval == EINPROGRESS)
    {
        return Status::NOT_READY;
    }

    if (optval != 0)
    {
        errno = optval;
        perror("getsockopt:isConnected:optval");
        return Status::ERROR;
    }

    return Status::DONE;
}

TcpSocket::Status TcpSocket::listen()
{
    assert(m_is_bound);
    int return_code = ::listen(m_socket_fd, SOMAXCONN);
    if (return_code == 0)
    {
        return Status::DONE;
    }

    perror("listen");
    return Status::ERROR;
}

TcpSocket::Status TcpSocket::listen(const uint16_t port)
{
    auto status = bind(port);
    if (status == Status::ERROR)
    {
        return status;
    }

    return listen();
}

TcpSocket::Status TcpSocket::bind(const IpAddress& remoteAddress)
{
    assert(!m_is_bound);

    addrinfo* address = remoteAddress.get_address_info();
    int return_code = ::bind(m_socket_fd, address->ai_addr, address->ai_addrlen);
    if (return_code == 0)
    {
        m_is_bound = true;
        return Status::DONE;
    }

    perror("bind");
    return Status::ERROR;
}

TcpSocket::Status TcpSocket::bind(const uint16_t port) { return bind(IpAddress("", port)); }

TcpSocket::Status TcpSocket::accept(TcpSocket* client)
{
    sockaddr in_address;
    socklen_t in_length = sizeof(in_address);
    int file_descriptor = ::accept4(m_socket_fd, &in_address, &in_length, SOCK_NONBLOCK);
    if (file_descriptor == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return Status::NOT_READY;
        }

        perror("accept");
        return Status::ERROR;
    }

    char hbuf[NI_MAXHOST];
    char sbuf[NI_MAXSERV];

    int return_code = ::getnameinfo(&in_address, in_length, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    if (return_code != 0)
    {
        std::cerr << "getnameinfo fail\n";
        return Status::ERROR;
    }

    client->m_socket_fd = file_descriptor;
    client->m_remote_host = hbuf;
    client->m_remote_port = std::stoul(sbuf);
    return Status::DONE;
}

TcpSocket::Status TcpSocket::send(const char* data, const std::size_t size, std::size_t* sent)
{
    int code = ::send(m_socket_fd, data, size, 0);
    if (code == -1)
    {
        if (errno == EAGAIN)
        {
            return Status::NOT_READY;
        }

        perror("send");
        return Status::ERROR;
    }

    *sent = code;
    return Status::DONE;
}

TcpSocket::Status TcpSocket::receive(char* data, const std::size_t size, std::size_t* received)
{
    int code = ::recv(m_socket_fd, data, size, 0);
    if (code == -1)
    {
        if (errno != EAGAIN)
        {
            perror("receive");
            return Status::ERROR;
        }

        return Status::NOT_READY;
    }

    *received = code;
    return Status::DONE;
}

uint16_t TcpSocket::getRemotePort() const { return m_remote_port; }

std::string TcpSocket::getRemoteAddress() const { return m_remote_host; }
