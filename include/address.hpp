#pragma once
#include "types.hpp"

namespace Net {

/**
 * @brief Represents a network address (IPv4 or IPv6) paired with a port number.
 *
 * Wraps a @c sockaddr_storage internally, which is large enough to hold either
 * an IPv4 (@c sockaddr_in) or IPv6 (@c sockaddr_in6) address.
 *
 * Direct construction is intentionally limited. Use the static @c from() factory
 * methods to build a valid instance. The default constructor produces an
 * uninitialized address; calling @c getIp() or @c getPort() on it will return
 * an error.
 *
 * @note All methods are @c noexcept — errors are communicated through
 *       @c Result<T> (i.e., @c std::expected) rather than exceptions.
 */
class Address {
public:

    /**
     * @brief Constructs an Address pre-sized for the given IP version.
     *
     * Does not populate the address data — only sets @c size_ to the correct
     * value for @p ipType  so that we can use it when we use @c recvFrom()
     * or @c sendTo() with this address.
     *
     * If @p ipType cannot be resolved (e.g. an unknown enum value),
     * @c size_ falls back to @c sizeof(sockaddr_in).
     *
     * @param ipType  The desired IP version (@c IPType::V4 or @c IPType::V6).
     *
     * @return Nothing — this is a constructor.
     * @throws Nothing — marked @c noexcept.
     */
    Address(const IPType ipType) noexcept;

    /**
     * @brief Creates an Address from a human-readable IP string and port.
     *
     * Tries IPv4 first via @c inet_pton(AF_INET, ...). If that fails, tries
     * IPv6 via @c inet_pton(AF_INET6, ...). The port is stored in network
     * byte order internally via @c htons().
     *
     * @param ip    A null-terminated string in dotted-decimal IPv4 format
     *              (e.g. @c "192.168.1.1") or colon-separated IPv6 format
     *              (e.g. @c "::1" or @c "2001:db8::1").
     * @param port  The port number in host byte order. Must be non-zero.
     *
     * @return On success, a @c Result<Address> holding the populated Address.
     * @return @c std::unexpected{Error::InvalidPort} if @p port is @c 0.
     * @return @c std::unexpected{Error::InvalidIP} if @p ip does not parse
     *         as either a valid IPv4 or IPv6 address.
     *
     * @throws Nothing — marked @c noexcept.
     */
    static Result<Address> from(const std::string& ip, uint16_t port) noexcept;

    /**
     * @brief Creates an Address from a generic @c sockaddr_storage.
     *
     * Inspects @c ss_family and delegates to the appropriate overload:
     * - @c AF_INET  → @c from(const sockaddr_in&)
     * - anything else → @c from(const sockaddr_in6&)
     *
     * Intended for use after OS calls such as @c accept() or @c recvfrom()
     * that write into a @c sockaddr_storage whose family is determined at
     * runtime.
     *
     * @param address  A populated @c sockaddr_storage from the OS.
     *
     * @return A fully populated @c Address matching the family in @p address.
     *
     * @throws Nothing — marked @c noexcept.
     */
    static Address from(const sockaddr_storage& address) noexcept;

    /**
     * @brief Creates an Address from a populated @c sockaddr_in (IPv4).
     *
     * Copies @c sin_family, @c sin_port, and @c sin_addr directly — no
     * byte-order conversion is performed, so the values must already be in
     * network byte order (as the OS provides them).
     * Sets @c size_ to @c sizeof(sockaddr_in).
     *
     * @param address  A valid, populated @c sockaddr_in struct.
     *
     * @return A fully populated IPv4 @c Address.
     *
     * @throws Nothing — marked @c noexcept.
     */
    static Address from(const sockaddr_in& address) noexcept;

    /**
     * @brief Creates an Address from a populated @c sockaddr_in6 (IPv6).
     *
     * Copies @c sin6_family, @c sin6_port, @c sin6_addr, and @c sin6_scope_id
     * directly without byte-order conversion.
     * Sets @c size_ to @c sizeof(sockaddr_in6).
     *
     * @param address  A valid, populated @c sockaddr_in6 struct.
     *
     * @return A fully populated IPv6 @c Address.
     *
     * @throws Nothing — marked @c noexcept.
     */
    static Address from(const sockaddr_in6& address) noexcept;

    /**
     * @brief Returns the IP address as a human-readable string.
     *
     * Uses @c inet_ntop() internally. For IPv4 addresses the result is in
     * dotted-decimal notation (e.g. @c "192.168.1.1"); for IPv6 in
     * colon-separated notation (e.g. @c "::1").
     *
     * The string is written into a @c thread_local buffer, so the returned
     * reference is valid until the next call to @c getIp() on any @c Address
     * from the same thread.
     *
     * @return On success, a @c Result holding a @c const @c std::string with
     *         the IP in printable form.
     * @return @c std::unexpected{Error::InvalidIP} if @c size_ is @c 0
     *         (uninitialized address).
     * @return @c std::unexpected with the platform error (from @c getError())
     *         if @c inet_ntop() fails.
     *
     * @throws Nothing — marked @c noexcept.
     */
    const Result<const std::string> getIp() const noexcept;

    /**
     * @brief Returns the port number in host byte order.
     *
     * Reads @c sin_port (IPv4) or @c sin6_port (IPv6) and converts from
     * network byte order using @c ntohs().
     *
     * @return On success, a @c Result<uint16_t> with the port in host byte
     *         order (1–65535).
     * @return @c std::unexpected{Error::InvalidPort} if @c size_ is @c 0
     *         (uninitialized address).
     *
     * @throws Nothing — marked @c noexcept.
     */
    Result<uint16_t> getPort() const noexcept;

    /**
     * @brief Returns whether this address is IPv4 or IPv6.
     *
     * Delegates to @c fromAddressFamily(address_.ss_family).
     *
     * @return A @c Result<IPType> indicating @c IPType::V4 or @c IPType::V6,
     *         or an error if the family is unrecognized.
     *
     * @throws Nothing — marked @c noexcept.
     */
    Result<IPType> getIpType() const noexcept {
        return fromAddressFamily(address_.ss_family);
    }

    /**
     * @brief Returns a read-only pointer to the raw @c sockaddr struct.
     *
     * Casts the internal @c sockaddr_storage to @c const @c sockaddr*.
     * Pass this pointer — together with @c getSize() — to OS socket calls
     * such as @c bind(), @c connect(), or @c sendto().
     *
     * @return A non-null @c const @c sockaddr* pointing into this object's
     *         internal storage. The pointer is valid for the lifetime of this
     *         @c Address instance.
     *
     * @throws Nothing — marked @c noexcept.
     */
    const sockaddr* getAddrRaw() const noexcept {
        return reinterpret_cast<const sockaddr*>(&address_);
    }

    /**
     * @brief Returns a mutable pointer to the raw @c sockaddr struct.
     *
     * Casts the internal @c sockaddr_storage to @c sockaddr*. Use this
     * overload when an OS call (e.g. @c accept(), @c recvfrom()) needs to
     * write the peer address back into this object.
     *
     * @return A non-null @c sockaddr* pointing into this object's internal
     *         storage. The pointer is valid for the lifetime of this
     *         @c Address instance.
     *
     * @throws Nothing — marked @c noexcept.
     */
    sockaddr* getAddrRaw() noexcept {
        return reinterpret_cast<sockaddr*>(&address_);
    }

    /**
     * @brief Returns the size of the active address struct in bytes.
     *
     * Returns @c sizeof(sockaddr_in) for IPv4 addresses and
     * @c sizeof(sockaddr_in6) for IPv6 addresses. Pass this value as the
     * @c addrlen argument to OS socket calls alongside @c getAddrRaw().
     *
     * @return The byte length of the underlying address struct as a
     *         @c socklen_t.
     *
     * @throws Nothing — marked @c noexcept.
     */
    socklen_t getSize() const noexcept {
        return size_;
    }

    /**
     * @brief Returns a mutable pointer to the address length field.
     *
     * Some OS calls (e.g. @c recvfrom(), @c accept()) take a @c socklen_t*
     * and write the actual address size back through it. Pass this pointer
     * so that @c size_ stays in sync after such a call.
     *
     * @return A non-null @c socklen_t* pointing to the internal @c size_
     *         member. The pointer is valid for the lifetime of this
     *         @c Address instance.
     *
     * @throws Nothing — marked @c noexcept.
     */
    socklen_t* getSizeRaw() noexcept {
        return &size_;
    }

    /**
     * @brief Default constructor — produces an uninitialized Address.
     *
     * @c size_ is set to @c sizeof(sockaddr_in6) and @c address_ is
     * zero-initialized. Calling @c getIp() or @c getPort() on this object
     * will return an error until it is populated by an OS call via
     * @c getAddrRaw() / @c getSizeRaw().
     */
    Address() = default;

private:
    /// Byte length of the active address struct (sizeof sockaddr_in or sockaddr_in6).
    socklen_t size_{ sizeof(sockaddr_in6) };

    /// Storage large enough for both IPv4 and IPv6 address structs.
    sockaddr_storage address_{};
};

} // namespace Net
