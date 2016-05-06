#include "ipaddress.hpp"
#include <algorithm>
#include <iostream>
#include <arpa/inet.h>

namespace
{

addrinfo* dns_lookup(const std::string& address, const uint16_t port)
{
    auto hints = std::make_unique<addrinfo>();

    // We must fill address by zeros
    std::fill_n(reinterpret_cast<char*>(hints.get()), sizeof(addrinfo), 0);

    hints->ai_family = AF_INET; // Allow IPv4
    hints->ai_socktype = SOCK_STREAM; // Tcp socket

    addrinfo* addr_info = nullptr;
    int status = 0;

    std::string service(std::to_string(port));
    status = getaddrinfo(address.empty() ? 0 : address.c_str(), service.c_str(), hints.get(), &addr_info);
    if (0 != status)
    {
        std::cerr << "error in IpAddress\n";
        return nullptr;
    }

    return addr_info;
}

}

IpAddress::IpAddress(const std::string& address, const uint16_t port)
    : m_address_info(dns_lookup(address, port))
{}

IpAddress::~IpAddress()
{
    if (m_address_info != nullptr) { freeaddrinfo(m_address_info); }
}

addrinfo *IpAddress::get_address_info() const { return m_address_info; }
