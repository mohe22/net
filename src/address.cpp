#include "../include/address.hpp"
#include <arpa/inet.h>
#include <cstring>

namespace Net
{

Address::Address(const IPType ipType) noexcept
{
    if (ipType == IPType::IPv4) {
        size_ = sizeof(sockaddr_in);
    } else if (ipType == IPType::IPv6) {
        size_ = sizeof(sockaddr_in6);
    } else {
        size_ = sizeof(sockaddr_in);
    }
    std::memset(&address_, 0, sizeof(address_));
}

Result<Address> Address::from(const std::string &ip, uint16_t port) noexcept
{
    if (port == 0)
        return std::unexpected{Error::InvalidPort};

    sockaddr_in addr4{};
    sockaddr_in6 addr6{};

    if (inet_pton(AF_INET, ip.c_str(), &addr4.sin_addr) == 1) {
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons(port);
        return from(addr4);
    } else if (inet_pton(AF_INET6, ip.c_str(), &addr6.sin6_addr) == 1) {
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(port);
        return from(addr6);
    }

    return std::unexpected{Error::InvalidIP};
}

Result<Address> Address::from(const sockaddr_storage &ss) noexcept
{
    switch (ss.ss_family) {
        case AF_INET:
            return from(*reinterpret_cast<const sockaddr_in*>(&ss));
        case AF_INET6:
            return from(*reinterpret_cast<const sockaddr_in6*>(&ss));
        default:
            return std::unexpected{Error::InvalidIP};
    }
}

Result<Address> Address::from(const sockaddr_in &address) noexcept
{
    Address a(IPType::IPv4);
    a.size_ = sizeof(sockaddr_in);
    std::memcpy(&a.address_, &address, sizeof(address));
    char buffer[INET_ADDRSTRLEN] = {};
    if (!inet_ntop(AF_INET, &address.sin_addr, buffer, sizeof(buffer))) {
        return std::unexpected{Error::InvalidIP};
    }
    a.ip_ = buffer;
    return a;
}

Result<Address> Address::from(const sockaddr_in6 &address) noexcept
{
    Address a(IPType::IPv6);
    a.size_ = sizeof(sockaddr_in6);
    std::memcpy(&a.address_, &address, sizeof(address));
    char buffer[INET6_ADDRSTRLEN] = {};
    if (!inet_ntop(AF_INET6, &address.sin6_addr, buffer, sizeof(buffer))) {
        return std::unexpected{Error::InvalidIP};
    }
    a.ip_ = buffer;
    return a;
}

const Result<const std::string_view> Address::getIp() const noexcept
{
    if (size_ == 0)
        return std::unexpected{Error::InvalidIP};
    return std::string_view(ip_);
}

Result<uint16_t> Address::getPort() const noexcept
{
    if (size_ == 0)
        return std::unexpected{Error::InvalidPort};

    if (size_ == sizeof(sockaddr_in)) {
        return ntohs(reinterpret_cast<const sockaddr_in*>(&address_)->sin_port);
    } else {
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&address_)->sin6_port);
    }
}

Result<IPType> Address::getIpType() const noexcept
{
    if (size_ == sizeof(sockaddr_in))
        return IPType::IPv4;
    else if (size_ == sizeof(sockaddr_in6))
        return IPType::IPv6;
    return std::unexpected{Error::InvalidIP};
}

const sockaddr *Address::getAddrRaw() const noexcept
{
    return reinterpret_cast<const sockaddr*>(&address_);
}


socklen_t Address::getSize() const noexcept
{
    return size_;
}



} // namespace Net
