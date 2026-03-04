#pragma once
#include "types.hpp"
#include <string>

namespace Net
{

    /**
     * @brief Represents a network address (IPv4 or IPv6) paired with a port number.
     *
     * Wraps a @c sockaddr_storage internally, which is large enough to hold either
     * an IPv4 (@c sockaddr_in) or IPv6 (@c sockaddr_in6) address.
     *
     * Direct construction is intentionally limited. Use the static @c from() factory
     * methods to build a valid instance. All factory methods return @c Result<Address>
     * and communicate failure through @c std::unexpected rather than exceptions.
     *
     * The default constructor produces an uninitialized address whose @c size_ is
     * set to @c sizeof(sockaddr_in6) — calling @c getIp() or @c getPort() on it
     * will return an error until it is populated by an OS call via
     * @c getAddrRaw() / @c getSizeRaw().
     *
     * The human-readable IP string is stored in the @c ip_ member and populated
     * once at construction time. The @c string_view returned by @c getIp() points
     * into @c ip_ and is valid for the lifetime of the @c Address instance.
     *
     * @note All methods are @c noexcept — errors are communicated through
     *       @c Result<T> (i.e. @c std::expected) rather than exceptions.
     */
    class Address
    {
    public:
        /**
         * @brief Constructs an Address pre-sized for the given IP version.
         *
         * Does not populate the address data — only sets @c size_ to the correct
         * value for @p ipType so that the struct can be passed directly to OS calls
         * such as @c recvfrom() or @c sendto() which need a pre-sized @c socklen_t.
         *
         * If @p ipType cannot be resolved (e.g. an unknown enum value),
         * @c size_ falls back to @c sizeof(sockaddr_in).
         *
         * @param ipType  The desired IP version (@c IPType::IPv4 or @c IPType::IPv6).
         *
         * @throws Nothing — marked @c noexcept.
         */
        Address(const IPType ipType) noexcept;

        /**
         * @brief Default constructor — produces an uninitialized Address.
         *
         * @c size_ is set to @c sizeof(sockaddr_in6) and @c address_ is
         * zero-initialized. @c ip_ is empty. Calling @c getIp() or @c getPort()
         * on this object will return an error until it is populated by an OS call
         * via @c getAddrRaw() / @c getSizeRaw().
         *
         * @throws Nothing — marked @c noexcept.
         */
        Address() = default;

        /**
         * @brief Creates an Address from a human-readable IP string and port.
         *
         * Tries IPv4 first via @c inet_pton(AF_INET, ...). If that fails, tries
         * IPv6 via @c inet_pton(AF_INET6, ...). The port is stored in network
         * byte order internally via @c htons(). The IP string is copied directly
         * into @c ip_ without going through @c inet_ntop().
         *
         * @param ip    Dotted-decimal IPv4 (e.g. @c "192.168.1.1") or
         *              colon-separated IPv6 (e.g. @c "::1", @c "2001:db8::1").
         * @param port  Port number in host byte order. Must be non-zero.
         *
         * @return On success, a @c Result<Address> holding the populated Address.
         * @return @c std::unexpected{Error::InvalidPort} if @p port is @c 0.
         * @return @c std::unexpected{Error::InvalidIP} if @p ip does not parse
         *         as either a valid IPv4 or IPv6 address.
         *
         * @throws Nothing — marked @c noexcept.
         */
        static Result<Address> from(const std::string &ip, uint16_t port) noexcept;

        /**
         * @brief Creates an Address from a generic @c sockaddr_storage.
         *
         * Inspects @c ss_family and delegates to the appropriate typed overload:
         * - @c AF_INET  → @c from(const sockaddr_in&)
         * - anything else → @c from(const sockaddr_in6&)
         *
         * Intended for use after OS calls such as @c accept() or @c recvfrom()
         * that write a peer address into a @c sockaddr_storage whose family is
         * determined at runtime.
         *
         * @param address  A populated @c sockaddr_storage from the OS.
         *
         * @return On success, a @c Result<Address> holding the populated Address.
         * @return @c std::unexpected{Error::InvalidIP} if @c inet_ntop() fails
         *         inside the delegated overload.
         *
         * @throws Nothing — marked @c noexcept.
         */
        static Result<Address> from(const sockaddr_storage &address) noexcept;

        /**
         * @brief Creates an Address from a populated @c sockaddr_in (IPv4).
         *
         * Copies @c sin_family, @c sin_port, and @c sin_addr directly — no
         * byte-order conversion is performed, so the values must already be in
         * network byte order (as the OS provides them after @c accept() or
         * @c recvfrom()). Uses @c inet_ntop() to convert the binary address into
         * a human-readable string stored in @c ip_.
         *
         * @param address  A valid, OS-populated @c sockaddr_in struct.
         *
         * @return On success, a @c Result<Address> holding the populated Address.
         * @return @c std::unexpected{Error::InvalidIP} if @c inet_ntop() fails.
         *
         * @throws Nothing — marked @c noexcept.
         */
        static Result<Address> from(const sockaddr_in &address) noexcept;

        /**
         * @brief Creates an Address from a populated @c sockaddr_in6 (IPv6).
         *
         * Copies @c sin6_family, @c sin6_port, @c sin6_addr, and @c sin6_scope_id
         * directly — no byte-order conversion is performed. Uses @c inet_ntop()
         * to convert the binary address into a human-readable string stored in
         * @c ip_.
         *
         * @param address  A valid, OS-populated @c sockaddr_in6 struct.
         *
         * @return On success, a @c Result<Address> holding the populated Address.
         * @return @c std::unexpected{Error::InvalidIP} if @c inet_ntop() fails.
         *
         * @throws Nothing — marked @c noexcept.
         */
        static Result<Address> from(const sockaddr_in6 &address) noexcept;

        /**
         * @brief Returns the IP address as a human-readable string.
         *
         * Returns a @c string_view into the @c ip_ member. The string is
         * populated once at construction time by the @c from() factory methods —
         * no computation is performed on each call.
         *
         * For IPv4 the string is in dotted-decimal notation (e.g. @c "192.168.1.1");
         * for IPv6 in colon-separated notation (e.g. @c "::1").
         *
         * The returned @c string_view is valid for the lifetime of this @c Address
         * instance. It is invalidated if the @c Address object is destroyed or
         * overwritten.
         *
         * @return On success, a @c Result holding a @c std::string_view into
         *         @c ip_ with the IP in human-readable form.
         * @return @c std::unexpected{Error::InvalidIP} if @c size_ is @c 0
         *         (uninitialized or failed address).
         *
         * @throws Nothing — marked @c noexcept.
         */
        const Result<const std::string_view> getIp() const noexcept;

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
         * @return @c Result<IPType::IPv4> or @c Result<IPType::IPv6> on success.
         * @return @c std::unexpected with an error if the family is unrecognized.
         *
         * @throws Nothing — marked @c noexcept.
         */
        Result<IPType> getIpType() const noexcept
        {
            return fromAddressFamily(address_.ss_family);
        }

        /**
         * @brief Returns a read-only pointer to the raw @c sockaddr struct.
         *
         * Casts the internal @c sockaddr_storage to @c const @c sockaddr*.
         * Pass this pointer — together with @c getSize() — to OS socket calls
         * such as @c bind(), @c connect(), or @c sendto().
         *
         * @return A non-null @c const @c sockaddr* valid for the lifetime of
         *         this @c Address instance.
         *
         * @throws Nothing — marked @c noexcept.
         */
        const sockaddr *getAddrRaw() const noexcept
        {
            return reinterpret_cast<const sockaddr *>(&address_);
        }

        /**
         * @brief Returns a mutable pointer to the raw @c sockaddr struct.
         *
         * Casts the internal @c sockaddr_storage to @c sockaddr*. Use this
         * overload when an OS call (e.g. @c accept(), @c recvfrom()) needs to
         * write the peer address back into this object.
         *
         * @return A non-null @c sockaddr* valid for the lifetime of this
         *         @c Address instance.
         *
         * @throws Nothing — marked @c noexcept.
         */
        sockaddr *getAddrRaw() noexcept
        {
            return reinterpret_cast<sockaddr *>(&address_);
        }

        /**
         * @brief Returns the size of the active address struct in bytes.
         *
         * Returns @c sizeof(sockaddr_in) for IPv4 and @c sizeof(sockaddr_in6)
         * for IPv6. Pass this value as the @c addrlen argument to OS socket
         * calls alongside @c getAddrRaw().
         *
         * @return The byte length of the underlying address struct as @c socklen_t.
         *
         * @throws Nothing — marked @c noexcept.
         */
        socklen_t getSize() const noexcept
        {
            return size_;
        }

        /**
         * @brief Returns a mutable pointer to the address length field.
         *
         * OS calls such as @c recvfrom() and @c accept() take a @c socklen_t*
         * and write the actual address size back through it. Pass this pointer
         * so that @c size_ stays in sync after such a call.
         *
         * @return A non-null @c socklen_t* pointing to @c size_, valid for the
         *         lifetime of this @c Address instance.
         *
         * @throws Nothing — marked @c noexcept.
         */
        socklen_t *getSizeRaw() noexcept
        {
            return &size_;
        }

    private:
        /// Byte length of the active address struct (sizeof sockaddr_in or sockaddr_in6).
        /// Set to 0 to mark the address as invalid (e.g. if inet_ntop() fails).
        socklen_t size_{sizeof(sockaddr_in6)};

        /// Storage large enough for both IPv4 and IPv6 address structs.
        sockaddr_storage address_{};

        /// Human-readable IP string.
        std::string ip_{};
    };

} // namespace Net