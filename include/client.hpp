#pragma once
#include "types.hpp"
#include "address.hpp"
#include "socketOptions.hpp"
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
class Client: public SocketOptions {
public:

    /**
     * @brief Default constructor — produces an uninitialized Client.
     *
     * @c socket_ is set to @c invaliedSocket and @c address_ is
     * default-initialized. Calling @c send(), @c receive(), or @c close()
     * on this object results in an error until a valid socket is assigned.
     */
    Client() = default;

    /**
     * @brief Destructor — closes the socket if still open.
     *
     * If @c socket_ is not @c invaliedSocket, calls @c platformClose() on it.
     * Always logs a debug message to stdout with the remote IP and port,
     * regardless of whether the socket was already closed.
     *
     * @throws Nothing — marked @c noexcept.
     */
    ~Client() noexcept;


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
    Client(SocketHandle socket, const Address& address) noexcept;

    /// @brief Deleted — socket ownership cannot be shared.
    Client(const Client&) = delete;

    /// @brief Deleted — socket ownership cannot be transferred.
    Client(Client&&) = delete;

    /// @brief Deleted — socket ownership cannot be shared.
    Client& operator=(const Client&) = delete;

    /// @brief Deleted — socket ownership cannot be transferred.
    Client& operator=(Client&&) = delete;



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
    Result<ssize> send(const void* data, size_t size);

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
    Result<ssize> receive(void* data, size_t size);

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
    Result<void> close();

    /// The remote address this client is connected to.
    Address address_{};

private:
    SocketHandle getSocket() const noexcept override { return socket_; }
    /// The underlying platform socket handle. @c invaliedSocket when not connected.
    SocketHandle socket_{ invalidSocket };

};

} // namespace Net
