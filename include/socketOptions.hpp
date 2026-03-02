#pragma once
#include "types.hpp"
#include <chrono>

namespace Net {

/**
 * @brief Mixin that provides socket option methods to any class that owns a socket.
 *
 * Inherit from this class and implement @c getSocket() to gain all socket
 * configuration methods — timeouts, buffer sizes, keep-alive, Nagle, broadcast,
 * and address reuse — without duplicating any logic.
 *
 * Typical usage:
 * @code
 * class MySocket : public Net::SocketOptions {
 * protected:
 *     SocketHandle getSocket() const noexcept override { return socket_; }
 * };
 * @endcode
 *
 * @note This class has no data members and no socket ownership.
 *       It only calls @c getSocket() at the moment an option is set,
 *       so the handle is always current even after a move or reconnect.
 */
class SocketOptions {
public:

    /**
     * @brief Sets the receive timeout for the socket.
     *
     * After this timeout elapses with no data received, @c recv() / @c recvfrom()
     * will return with @c Error::WouldBlock instead of blocking indefinitely.
     *
     * @param timeout Duration in milliseconds. Pass @c 0ms to disable (block forever).
     *
     * @return @c Result<void> with no value on success.
     * @return @c std::unexpected with the platform error on failure.
     *
     * @throws Nothing — marked @c noexcept.
     */
    Result<void> setReceiveTimeout(std::chrono::milliseconds timeout) const noexcept {
        return setTimeoutOption(SocketOption::ReceiveTimeout, timeout);
    }

    /**
     * @brief Sets the send timeout for the socket.
     *
     * After this timeout elapses with no data sent, @c send() / @c sendto()
     * will return with @c Error::WouldBlock instead of blocking indefinitely.
     *
     * @param timeout Duration in milliseconds. Pass @c 0ms to disable (block forever).
     *
     * @return @c Result<void> with no value on success.
     * @return @c std::unexpected with the platform error on failure.
     *
     * @throws Nothing — marked @c noexcept.
     */
    Result<void> setSendTimeout(std::chrono::milliseconds timeout) const noexcept {
        return setTimeoutOption(SocketOption::SendTimeout, timeout);
    }

    /**
     * @brief Sets the receive buffer size for the socket.
     *
     * Controls how many bytes the OS will buffer for incoming data before
     * dropping packets. The kernel may internally double the requested value.
     *
     * @param size Buffer size in bytes.
     *
     * @return @c Result<void> with no value on success.
     * @return @c std::unexpected with the platform error on failure.
     *
     * @throws Nothing — marked @c noexcept.
     */
    Result<void> setReceiveBuffer(int size) const noexcept {
        return setOption(SocketOption::ReceiveBuffer, &size, sizeof(size));
    }

    /**
     * @brief Sets the send buffer size for the socket.
     *
     * Controls how many bytes the OS will buffer for outgoing data before
     * blocking the caller. The kernel may internally double the requested value.
     *
     * @param size Buffer size in bytes.
     *
     * @return @c Result<void> with no value on success.
     * @return @c std::unexpected with the platform error on failure.
     *
     * @throws Nothing — marked @c noexcept.
     */
    Result<void> setSendBuffer(int size) const noexcept {
        return setOption(SocketOption::SendBuffer, &size, sizeof(size));
    }

    /**
     * @brief Allows the socket to bind to an address already in use.
     *
     * Sets @c SO_REUSEADDR. Useful for servers that need to restart quickly
     * without waiting for the OS @c TIME_WAIT period to expire.
     *
     * @param enable @c true to enable, @c false to disable. Defaults to @c true.
     *
     * @return @c Result<void> with no value on success.
     * @return @c std::unexpected with the platform error on failure.
     *
     * @throws Nothing — marked @c noexcept.
     */
    Result<void> setReuseAddress(bool enable = true) const noexcept {
        int val = enable ? 1 : 0;
        return setOption(SocketOption::ReuseAddress, &val, sizeof(val));
    }

    /**
     * @brief Allows multiple sockets to bind to the same port simultaneously.
     *
     * Sets @c SO_REUSEPORT. Useful for multi-threaded servers where each thread
     * owns its own socket on the same port and the OS load-balances between them.
     *
     * @param enable @c true to enable, @c false to disable. Defaults to @c true.
     *
     * @return @c Result<void> with no value on success.
     * @return @c std::unexpected with the platform error on failure.
     *
     * @throws Nothing — marked @c noexcept.
     */
    Result<void> setReusePort(bool enable = true) const noexcept {
        int val = enable ? 1 : 0;
        return setOption(SocketOption::ReusePort, &val, sizeof(val));
    }

    /**
     * @brief Enables keep-alive probes to detect dead connections.
     *
     * Sets @c SO_KEEPALIVE. When enabled the OS periodically sends probes on
     * idle connections and closes the socket if the remote peer stops responding.
     *
     * @param enable @c true to enable, @c false to disable. Defaults to @c true.
     *
     * @return @c Result<void> with no value on success.
     * @return @c std::unexpected with the platform error on failure.
     *
     * @throws Nothing — marked @c noexcept.
     */
    Result<void> setKeepAlive(bool enable = true) const noexcept {
        int val = enable ? 1 : 0;
        return setOption(SocketOption::KeepAlive, &val, sizeof(val));
    }

    /**
     * @brief Disables Nagle's algorithm for lower latency.
     *
     * Sets @c TCP_NODELAY. When enabled, small writes are sent immediately
     * rather than being held and coalesced. Reduces latency at the cost of
     * potentially higher packet count.
     *
     * @param enable @c true to disable Nagle (no delay), @c false to re-enable it.
     *               Defaults to @c true.
     *
     * @return @c Result<void> with no value on success.
     * @return @c std::unexpected with the platform error on failure.
     *
     * @throws Nothing — marked @c noexcept.
     */
    Result<void> setNoDelay(bool enable = true) const noexcept {
        int val = enable ? 1 : 0;
        return setOption(SocketOption::NoDelay, &val, sizeof(val));
    }

    /**
     * @brief Allows sending broadcast messages on the socket.
     *
     * Sets @c SO_BROADCAST. Required before calling @c sendTo() with a
     * broadcast address (e.g. @c "255.255.255.255"). Only meaningful on
     * UDP sockets.
     *
     * @param enable @c true to enable, @c false to disable. Defaults to @c true.
     *
     * @return @c Result<void> with no value on success.
     * @return @c std::unexpected with the platform error on failure.
     *
     * @throws Nothing — marked @c noexcept.
     */
    Result<void> setBroadcast(bool enable = true) const noexcept {
        int val = enable ? 1 : 0;
        return setOption(SocketOption::Broadcast, &val, sizeof(val));
    }

protected:

    /**
    * @brief Returns the socket handle — must be implemented by the subclass.
    * @return The current @c SocketHandle, or @c invalidSocket if not initialized.
    * @throws Nothing — marked @c noexcept.
    */
    virtual SocketHandle getSocket() const noexcept = 0;

private:

    /**
     * @brief Low-level wrapper around @c ::setsockopt().
     *
     * Retrieves the socket handle via @c getSocket(), resolves the correct
     * protocol level via @c getOptionLevel(), and forwards to the OS.
     * On Windows @p optval is cast to @c const char* as required by WinSock;
     * on POSIX it is passed as @c const void*.
     *
     * @param option    The option to set — resolves to a @c SO_* or @c TCP_* constant.
     * @param optval    Pointer to the value. May be @c int, @c DWORD, @c timeval, etc.
     * @param optlen    Size of @p optval in bytes.
     *
     * @return @c Result<void> with no value on success.
     * @return @c std::unexpected with the platform error if @c ::setsockopt() fails.
     *
     * @throws Nothing — marked @c noexcept.
     */
    Result<void> setOption(SocketOption option,
                           const void* optval, int optlen) const noexcept;

    /**
     * @brief Converts a @c std::chrono::milliseconds duration to the
     *        platform-native timeout format and calls @c setOption().
     *
     * On Windows the timeout is a @c DWORD of milliseconds.
     * On POSIX it is a @c struct @c timeval { tv_sec, tv_usec }.
     * The conversion is:
     * @code
     * tv_sec  = timeout / 1000
     * tv_usec = (timeout % 1000) * 1000
     * @endcode
     *
     * @param option   Must be @c SocketOption::ReceiveTimeout or
     *                 @c SocketOption::SendTimeout.
     * @param timeout  Duration in milliseconds. Pass @c 0ms to disable the timeout.
     *
     * @return @c Result<void> with no value on success.
     * @return @c std::unexpected with the platform error if @c ::setsockopt() fails.
     *
     * @throws Nothing — marked @c noexcept.
     */
    Result<void> setTimeoutOption(SocketOption option,
                                  std::chrono::milliseconds timeout) const noexcept;
};

} // namespace Net
