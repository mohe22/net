#pragma once
#include "platform.hpp"
#include "types.hpp"
#include "address.hpp"
#include "socketOptions.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
namespace Net {

/**
 * @brief Represents a connected TCP client socket.
 *
 * Owns a single @c SocketHandle and the remote @c Address associated with it.
 * The socket is automatically closed in the destructor if it has not been
 * explicitly closed beforehand via @c close().
 *
 * @note This class is non-copyable and non-movable. Each instance exclusively
 *       owns its socket — transfer of ownership is intentionally disallowed.
 *
 * @note All operations are blocking. @c send() and @c receive() will block
 *       until data is written/read or an error occurs.
 */
class Connection: public SocketOptions {

    public:
        /**
         * @brief Creates an outbound TCP connection to the specified remote endpoint.
         *
         * Resolves @p ip and @p port into an @c Address, creates a platform socket,
         * and calls @c ::connect(). On success, the resulting socket and address are
         * wrapped in a heap-allocated @c Client and returned to the caller.
         *
         * On Windows, @c WSAStartup() is called automatically before any socket
         * operations. On all platforms, if any step fails the socket is closed
         * before the error is returned — no resources are leaked.
         *
         * @param ip      The remote IP address to connect to, as a dotted-decimal
         *                string (e.g. @c "93.184.216.34" or @c "::1").
         * @param port    The remote port number to connect to, in host byte order.
         * @param ipType  The IP address family to use. Defaults to @c IPType::IPv4.
         *                Pass @c IPType::IPv6 for IPv6 connections.
         *
         * @return On success, a @c Result holding a @c std::unique_ptr<Client> that
         *         owns the connected socket. The caller is responsible for its lifetime.
         * @return @c std::unexpected{Error::WSAStartupFailed} if @c WSAStartup()
         *         fails (Windows only).
         * @return @c std::unexpected with an address-resolution error if @p ip or
         *         @p port cannot be parsed into a valid @c Address.
         * @return @c std::unexpected with the platform error (from @c getError()) if
         *         @c ::socket() fails to create a socket.
         * @return @c std::unexpected with the platform error (from @c getError()) if
         *         @c ::connect() fails to establish a connection (e.g. refused,
         *         timed out, unreachable).
         *
         * @throws Nothing — marked @c noexcept.
         */
        static Result<std::unique_ptr<Connection>> connect(
            const std::string &ip,
            uint16_t port,
            IPType ipType = IPType::IPv4) noexcept;

        /**
         * @brief Default constructor — produces an uninitialized Client.
         *
         * @c socket_ is set to @c invaliedSocket and @c address_ is
         * default-initialized. Calling @c send(), @c receive(), or @c close()
         * on this object results in an error until a valid socket is assigned.
         */
        Connection() = default;

        /**
         * @brief Destructor — closes the socket if still open.
         *
         * If @c socket_ is not @c invaliedSocket, calls @c platformClose() on it.
         * Always logs a debug message to stdout with the remote IP and port,
         * regardless of whether the socket was already closed.
         *
         * @throws Nothing — marked @c noexcept.
         */
        ~Connection() noexcept;

        /**
         * @brief Constructs a Client from an accepted socket and its remote address.
         *
         * Takes ownership of @p socket. Logs a debug message to stdout with the
         * remote IP and port upon construction.
         *
         * @param socket   A valid platform socket handle obtained from @c accept()
         *                 or equivalent. Must not be @c invaliedSocket.
         * @param address  The remote @c Address associated with this connection.
         *
         * @throws Nothing — marked @c noexcept.
         */
        Connection(SocketHandle socket, const Address &address) noexcept;

        /// @brief Deleted — socket ownership cannot be shared.
        Connection(const Connection &) = delete;

        /// @brief Deleted — socket ownership cannot be transferred.
        Connection(Connection &&) = delete;

        /// @brief Deleted — socket ownership cannot be shared.
        Connection &operator=(const Connection &) = delete;

        /// @brief Deleted — socket ownership cannot be transferred.
        Connection &operator=(Connection &&) = delete;


        /**
         * @brief Sends raw data to the remote peer.
         *
         * Calls the platform @c ::send() syscall. On Windows, @p data is cast to
         * @c const @c char* and @p size to @c int to satisfy the Winsock API.
         *
         * @param data  Pointer to the buffer containing the bytes to send.
         *              Must not be @c nullptr and must be at least @p size bytes.
         * @param size  Number of bytes to send from @p data.
         *
         * @return On success, a @c Result<ssize> holding the number of bytes
         *         actually sent, which may be less than @p size.
         * @return @c std::unexpected{Error::ConnectionClosed} if the peer has
         *         closed the connection (@c ::send() returned @c 0).
         * @return @c std::unexpected with the platform error (from @c getError())
         *         if @c ::send() returned @c -1.
         *
         * @throws Nothing.
         */
        Result<ssize> send(const void *data, size_t size) noexcept;

        /**
         * @brief Receives raw data from the remote peer into a caller-supplied buffer.
         *
         * Calls the platform @c ::recv() syscall. On Windows, @p data is cast to
         * @c char* and @p size to @c int to satisfy the Winsock API.
         *
         * @param data  Pointer to the buffer where received bytes will be written.
         *              Must not be @c nullptr and must be at least @p size bytes.
         * @param size  Maximum number of bytes to read into @p data.
         *
         * @return On success, a @c Result<ssize> holding the number of bytes
         *         actually received.
         * @return @c std::unexpected{Error::ConnectionClosed} if the peer has
         *         closed the connection gracefully (@c ::recv() returned @c 0).
         * @return @c std::unexpected with the platform error (from @c getError())
         *         if @c ::recv() returned @c -1.
         *
         * @throws Nothing.
         */
        Result<ssize> receive(void *data, size_t size) noexcept;


        /**
         * @brief Receives exactly @p totalBytes bytes from the remote peer into a
         *        caller-supplied buffer, looping over @c receive() until all bytes
         *        have arrived.
         *
         * Unlike @c receive(), which may return fewer bytes than requested in a
         * single call, @c receiveAll() blocks until the full @p totalBytes have
         * been read or an error occurs.
         *
         * @param data        A writable span over the destination buffer.
         *                    Must be at least @p totalBytes bytes in size.
         * @param totalBytes  The exact number of bytes to receive.
         *
         * @return On success, a @c Result<ssize> holding @p totalBytes.
         * @return @c std::unexpected{Error::BufferTooSmall} if @c data.size() is
         *         less than @p totalBytes — no I/O is performed.
         * @return @c std::unexpected{Error::ConnectionClosed} if the peer closes
         *         the connection before all @p totalBytes have been delivered.
         * @return @c std::unexpected with the platform error (from @c getError())
         *         if any underlying @c ::recv() call fails.
         *
         * @throws Nothing — marked @c noexcept.
         */
        Result<ssize> receiveAll(std::span<uint8_t> data, size_t totalBytes) noexcept;

        /**
         * @brief Sends exactly @p totalBytes bytes to the remote peer from a
         *        caller-supplied buffer, looping over @c send() until all bytes
         *        have been written.
         *
         * Unlike @c send(), which may transmit fewer bytes than requested in a
         * single call, @c sendAll() blocks until the full @p totalBytes have
         * been sent or an error occurs.
         *
         * @param data        A read-only span over the source buffer.
         *                    Must be at least @p totalBytes bytes in size.
         * @param totalBytes  The exact number of bytes to send.
         *
         * @return On success, a @c Result<ssize> holding @p totalBytes.
         * @return @c std::unexpected{Error::BufferTooSmall} if @c data.size() is
         *         less than @p totalBytes — no I/O is performed.
         * @return @c std::unexpected{Error::ConnectionClosed} if the peer closes
         *         the connection before all @p totalBytes have been transmitted.
         * @return @c std::unexpected with the platform error (from @c getError())
         *         if any underlying @c ::send() call fails.
         *
         * @throws Nothing — marked @c noexcept.
         */
        Result<ssize> sendAll(std::span<const uint8_t> data, size_t totalBytes) noexcept;


        /**
         * @brief Explicitly closes the socket.
         *
         * Calls @c platformClose() on @c socket_  and resets it to
         * @c invaliedSocket so that the destructor does not attempt to
         * close it a second time.
         *
         * @return @c Result<void> with no value on success.
         * @return @c std::unexpected with the platform error (from @c getError())
         *         if @c socket_ is already @c invaliedSocket (already closed or
         *         never initialized).
         *
         * @throws Nothing.
         */
        Result<void> close() noexcept;

        /*
            @brief Retrieves the remote IP address and port as strings.
            @retrun A tuple containing the IP address as a string_view and the port number as a uint16_t.
            @throws Nothing — marked @c noexcept.
        */
        std::tuple<std::string_view, uint16_t> getRemoteIpPort() const noexcept
        {
            return {address_.getIp().value_or("UnknownIP"),
                    address_.getPort().value_or(0)};
        }

        SocketHandle getSocket() const noexcept override { return socket_; }
private:

    /// The underlying platform socket handle. @c invaliedSocket when not connected.
    SocketHandle socket_{ invalidSocket };
    /// The remote address this client is connected to.
    Address address_{};
};

} // namespace Net
