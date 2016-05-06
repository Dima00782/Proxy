#ifndef IP_ADDRESS_HPP
#define IP_ADDRESS_HPP

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <cstdint>
#include <string>
#include <memory>

class IpAddress final
{
public:
    IpAddress(const std::string& address, const uint16_t port);
    ~IpAddress();

    addrinfo* get_address_info() const;

private:
    addrinfo* m_address_info;
};

#endif // IPADDRESS_HPP
