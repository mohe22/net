
#pragma once
#include <expected>
#include "error.hpp"


namespace Net {

    class Address;
    // Generic result wrapper for functions returning a value or error
    template <typename T> using Result = std::expected<T, Net::Error>;

    using RecvFromResult = std::tuple<ssize, Address>;


    // ?do i need this?!
    // Protocol type for socket
    enum class Protocol : uint8_t { TCP, UDP, RAW };
    // IP address type
    enum class IPType : uint8_t { IPv4, IPv6 };

    // ?do i need this?!
    inline Result<int> toSocketType(Protocol protocol) noexcept {
        switch (protocol) {
            case Protocol::TCP: return SOCK_STREAM;
            case Protocol::UDP: return SOCK_DGRAM;
            case Protocol::RAW: return SOCK_RAW;
        }
        return std::unexpected{Net::Error::InvalidSocketType};
    }

    inline Result<int> toAddressFamily(const IPType ipType)  noexcept {
        switch (ipType) {
            case IPType::IPv4: return AF_INET;
            case IPType::IPv6: return AF_INET6;
        }
        return std::unexpected{Error::InvalidAddressFamily};
    }

    inline Result<int> getAddressSizeByIpType(const IPType ipType)  noexcept {
        switch (ipType) {
            case IPType::IPv4: return sizeof(sockaddr_in);
            case IPType::IPv6: return sizeof(sockaddr_in6);
        }
        return std::unexpected{Error::InvalidAddressFamily};
    }
    inline Result<IPType> fromAddressFamily(const ssize ipType) noexcept {
        switch (ipType) {
            case AF_INET: return IPType::IPv4;
            case AF_INET6: return IPType::IPv6;
        }
        return std::unexpected{Net::Error::InvalidAddressFamily};
    }
}
