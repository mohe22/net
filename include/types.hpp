#pragma once
#include <expected>
#include "error.hpp"

namespace Net {

    class Address;

    /**
     * @brief Generic result type used throughout the Net library.
     *
     * Wraps @c std::expected<T, Net::Error>. On success, holds a value of
     * type @c T. On failure, holds a @c Net::Error via @c std::unexpected.
     *
     * @tparam T  The value type returned on success.
     *
     * Example usage:
     * @code
     * Result<uint16_t> port = address.getPort();
     * if (!port)
     *     std::println("Error: {}", Net::toErrorString(port.error()));
     * else
     *     std::println("Port: {}", port.value());
     * @endcode
     */
    template <typename T>
    using Result = std::expected<T, Net::Error>;

    /**
     * @brief Result type returned by @c Udp::receiveFrom().
     *
     * A tuple of:
     * - @c ssize — the number of bytes received.
     * - @c Address — the sender's address as populated by @c ::recvfrom().
     */
    using RecvFromResult = std::tuple<ssize, Address>;

    /**
     * @brief Identifies the transport-layer protocol for a socket.
     *
     * Passed to @c toSocketType() to resolve the corresponding @c SOCK_*
     * constant. @c RAW requires root or Administrator privileges at runtime.
     */
    enum class Protocol : uint8_t {
        TCP, ///< Transmission Control Protocol — reliable, connection-oriented (@c SOCK_STREAM).
        UDP, ///< User Datagram Protocol — connectionless, best-effort (@c SOCK_DGRAM).
        RAW  ///< Raw socket — direct access to the network layer (@c SOCK_RAW). Requires elevated privileges.
    };

    /**
     * @brief Identifies the IP version of an address or socket.
     */
    enum class IPType : uint8_t {
        IPv4, ///< Internet Protocol version 4 — 32-bit address (@c AF_INET).
        IPv6  ///< Internet Protocol version 6 — 128-bit address (@c AF_INET6).
    };

    /**
     * @brief Maps a @c Protocol value to its corresponding @c SOCK_* constant.
     *
     * Used during socket creation to pass the correct type to @c ::socket().
     *
     * @param protocol  The desired transport protocol.
     *
     * @return On success, a @c Result<int> holding @c SOCK_STREAM,
     *         @c SOCK_DGRAM, or @c SOCK_RAW.
     * @return @c std::unexpected{Error::InvalidSocketType} if @p protocol
     *         does not match any known enumerator.
     *
     * @throws Nothing — marked @c noexcept.
     */
    inline Result<int> toSocketType(Protocol protocol) noexcept {
        switch (protocol) {
            case Protocol::TCP: return SOCK_STREAM;
            case Protocol::UDP: return SOCK_DGRAM;
            case Protocol::RAW: return SOCK_RAW;
        }
        return std::unexpected{Net::Error::InvalidSocketType};
    }

    /**
     * @brief Maps an @c IPType to its corresponding address family constant.
     *
     * Used to resolve the @c int passed as the first argument to @c ::socket()
     * and other OS calls that accept an address family.
     *
     * @param ipType  The IP version to resolve.
     *
     * @return On success, a @c Result<int> holding @c AF_INET (IPv4) or
     *         @c AF_INET6 (IPv6).
     * @return @c std::unexpected{Error::InvalidAddressFamily} if @p ipType
     *         does not match any known enumerator.
     *
     * @throws Nothing — marked @c noexcept.
     */
    inline Result<int> toAddressFamily(const IPType ipType) noexcept {
        switch (ipType) {
            case IPType::IPv4: return AF_INET;
            case IPType::IPv6: return AF_INET6;
        }
        return std::unexpected{Error::InvalidAddressFamily};
    }

    /**
     * @brief Returns the size in bytes of the socket address struct for a given IP version.
     *
     * Used when pre-sizing an @c Address before passing it to OS calls that
     * write an address back (e.g. @c recvfrom(), @c accept()).
     *
     * @param ipType  The IP version to query.
     *
     * @return On success, a @c Result<int> holding @c sizeof(sockaddr_in)
     *         for IPv4 or @c sizeof(sockaddr_in6) for IPv6.
     * @return @c std::unexpected{Error::InvalidAddressFamily} if @p ipType
     *         does not match any known enumerator.
     *
     * @throws Nothing — marked @c noexcept.
     */
    inline Result<int> getAddressSizeByIpType(const IPType ipType) noexcept {
        switch (ipType) {
            case IPType::IPv4: return sizeof(sockaddr_in);
            case IPType::IPv6: return sizeof(sockaddr_in6);
        }
        return std::unexpected{Error::InvalidAddressFamily};
    }

    /**
     * @brief Maps a raw address family integer to its @c IPType equivalent.
     *
     * Typically called with @c sockaddr_storage::ss_family or
     * @c sockaddr_in::sin_family as returned by the OS after @c accept() or
     * @c recvfrom().
     *
     * @param ipType  A raw address family value (e.g. @c AF_INET or @c AF_INET6)
     *                stored as @c ssize to match the width of @c ss_family on
     *                all platforms.
     *
     * @return On success, a @c Result<IPType> holding @c IPType::IPv4 or
     *         @c IPType::IPv6.
     * @return @c std::unexpected{Error::InvalidAddressFamily} if @p ipType
     *         is not @c AF_INET or @c AF_INET6.
     *
     * @throws Nothing — marked @c noexcept.
     */
    inline Result<IPType> fromAddressFamily(const ssize ipType) noexcept {
        switch (ipType) {
            case AF_INET:  return IPType::IPv4;
            case AF_INET6: return IPType::IPv6;
        }
        return std::unexpected{Net::Error::InvalidAddressFamily};
    }

} // namespace Net
