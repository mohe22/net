#pragma once
#include "platform.hpp"
#include "types.hpp"
#include "client.hpp"
#include "socketOptions.hpp"


#include <memory> // for std::unique_ptr
#include <chrono> // for std::chrono::milliseconds


namespace Net {
    namespace Servers {
        /**
         * @brief Abstract base class providing shared socket lifecycle management.
         *
         * Owns a single @c SocketHandle and its associated @c Address. Handles
         * platform-level initialization (Winsock on Windows), binding, and cleanup.
         * Concrete subclasses (@c Tcp, @c Udp) provide the socket type via the
         * pure virtual @c socketType() and extend with protocol-specific operations.
         *
         * The destructor automatically calls @c closeSocket() and, on Windows,
         * @c WSACleanup() if Winsock was initialized via @c init().
         *
         * @note This class is not intended to be used directly — use @c Tcp or @c Udp.
         */
        class SocketBase: public Net::SocketOptions {
            public:

                

                /**
                 * @brief Initializes the socket for the given IP version.
                 *
                 * On Windows, initializes Winsock 2.2 via @c WSAStartup() before any
                 * socket call. Then resolves the address family from @p ipType, calls
                 * @c ::socket() with the result and the subclass-provided @c socketType(),
                 * and stores the handle in @c socket_.
                 *
                 * On Windows, calling @c init() a second time without an intervening
                 * cleanup returns @c Error::AlreadyConnected immediately.
                 *
                 * @param ipType  The IP version to create the socket for
                 *                (@c IPType::V4 or @c IPType::V6).
                 *
                 * @return @c Result<void> with no value on success.
                 * @return @c std::unexpected{Error::AlreadyConnected} on Windows if
                 *         @c WSAStartup() has already been called on this instance.
                 * @return @c std::unexpected{Error::WSAStartupFailed} on Windows if
                 *         @c WSAStartup() fails.
                 * @return @c std::unexpected{Error::InvalidAddressFamily} if @p ipType
                 *         cannot be mapped to an address family.
                 * @return @c std::unexpected with the platform error (from @c getError())
                 *         if @c ::socket() fails.
                 *
                 * @throws Nothing — marked @c noexcept.
                 */
                Result<void> init(IPType ipType) noexcept;

                /**
                 * @brief Binds the socket to a local IP address and port.
                 *
                 * Constructs an @c Address from @p ip and @p port via
                 * @c Address::from(), then calls @c ::bind(). On success, stores the
                 * address in @c address_ for later retrieval via @c getAddress().
                 *
                 * @param ip    A human-readable IPv4 or IPv6 address string to bind to
                 *              (e.g. @c "0.0.0.0" or @c "::").
                 * @param port  The local port to bind to, in host byte order. Must be
                 *              non-zero.
                 *
                 * @return @c Result<void> with no value on success.
                 * @return @c std::unexpected{Error::SocketNotInitialized} if @c init()
                 *         has not been called yet.
                 * @return @c std::unexpected{Error::InvalidIP} or
                 *         @c std::unexpected{Error::InvalidPort} if @p ip or @p port
                 *         are invalid (propagated from @c Address::from()).
                 * @return @c std::unexpected with the platform error (from @c getError())
                 *         if @c ::bind() fails (e.g. @c Error::AddressAlreadyInUse).
                 *
                 * @throws Nothing — marked @c noexcept.
                 */
                Result<void> bind(const std::string& ip, uint16_t port) noexcept;

                /**
                 * @brief Destructor — closes the socket and cleans up platform resources.
                 *
                 * Calls @c closeSocket(). On Windows, also calls @c ::WSACleanup() if
                 * @c wsaInitialized_ is @c true.
                 *
                 * @throws Nothing — marked @c noexcept.
                 */
                ~SocketBase() noexcept {
                    closeSocket();
                    #ifdef _WIN32
                        if (wsaInitialized_)
                            ::WSACleanup();
                    #endif
                }



                /**
                 * @brief Returns the underlying socket handle.
                 *
                 * @return The raw @c SocketHandle. May be @c invaliedSocket if @c init()
                 *         has not been called or @c closeSocket() has been called.
                 *
                 * @throws Nothing — marked @c noexcept.
                 */
                SocketHandle getSocket() const noexcept override { return socket_; }

                /**
                 * @brief Returns the local address this socket is bound to.
                 *
                 * The address is only meaningful after a successful call to @c bind().
                 * Before that it is default-initialized.
                 *
                 * @return A @c const reference to the internal @c Address. Valid for
                 *         the lifetime of this object.
                 *
                 * @throws Nothing — marked @c noexcept.
                 */
                const Address& getAddress() const noexcept { return address_; }

                /**
                 * @brief Returns the IP version of the bound address.
                 *
                 * Delegates to @c address_.getIpType(). Only meaningful after a
                 * successful call to @c bind().
                 *
                 * @return A @c Result<IPType> with @c IPType::V4 or @c IPType::V6 on
                 *         success, or an error if the address family is unrecognized.
                 *
                 * @throws Nothing — marked @c noexcept.
                 */
                const Result<IPType> getIpType() const noexcept { return address_.getIpType(); }

                /**
                 * @brief Returns whether the socket handle is currently valid.
                 *
                 * @return @c true if @c socket_ is not @c invaliedSocket, @c false
                 *         otherwise.
                 *
                 * @throws Nothing — marked @c noexcept.
                 */
                bool isValidSocket() const noexcept {
                    return socket_ != invalidSocket;
                }

                /**
                 * @brief Closes the socket and resets the handle to @c invaliedSocket.
                 *
                 * Calls @c platformClose() only if @c isValidSocket() is @c true, then
                 * unconditionally sets @c socket_ to @c invaliedSocket so that
                 * subsequent calls are safe no-ops.
                 *
                 * @throws Nothing — marked @c noexcept.
                 */
                void closeSocket() noexcept {
                    if (isValidSocket()) {
                        platformClose(socket_);
                    }
                    socket_ = invalidSocket;
                }





            protected:

                /**
                 * @brief Returns the socket type constant for this protocol.
                 *
                 * Implemented by subclasses to return the @c SOCK_* constant passed to
                 * @c ::socket() during @c init():
                 * - @c Tcp returns @c SOCK_STREAM
                 * - @c Udp returns @c SOCK_DGRAM
                 *
                 * @return A @c SOCK_* constant (e.g. @c SOCK_STREAM or @c SOCK_DGRAM).
                 *
                 * @throws Nothing — marked @c noexcept.
                 */
                virtual int socketType() const noexcept = 0;


            private:
                /**
                * @brief Sets a socket option on the underlying socket.
                * @param option  The socket option to set, see @c SocketOption enum.
                * @param optval  Pointer to the value to set. Can be any type (int, DWORD, struct timeval, etc).
                * @param optlen  Size of the value in bytes. Defaults to @c sizeof(int).
                * @return        A @c Result indicating success or failure.
                * @note          On Windows, @c optval is cast to @c const char* as required by WinSock.
                *                On Linux,   @c optval is cast to @c const int*  as required by POSIX.
                */
                Result<void> setOption(SocketOption option,const void* optval, int optlen = sizeof(int)) const noexcept;

                /**
                * @brief Sets a timeout option on the underlying socket.
                * @param option  The timeout option to set, either @c SocketOption::ReceiveTimeout
                *                or @c SocketOption::SendTimeout.
                * @param timeout The timeout duration in milliseconds. Pass @c 0ms to disable.
                * @return        A @c Result indicating success or failure.
                * @note          On Windows, the timeout is passed as a @c DWORD of milliseconds.
                *                On Linux,   the timeout is passed as a @c struct @c timeval { seconds, microseconds }.
                */
                Result<void> setTimeoutOption(SocketOption option,std::chrono::milliseconds timeout) const noexcept;

                /// The underlying platform socket handle. @c invaliedSocket when not initialized or closed.
                SocketHandle socket_{ invalidSocket };

                /// The local address this socket is bound to. Populated by @c bind().
                Address address_{};

                #ifdef _WIN32
                    /// Windows only — tracks whether @c WSAStartup() has been called on this instance.
                    bool wsaInitialized_{ false };
                #endif
            };


        /**
         * @brief UDP socket server (@c SOCK_DGRAM).
         *
         * Extends @c SocketBase with connectionless datagram operations.
         * Each @c sendTo() and @c receiveFrom() call carries its own destination
         * or source address — no @c connect() or @c accept() step is required.
         *
         * Non-copyable. Movable via the default move constructor and assignment.
         */
        class Udp : public SocketBase {
        public:

            /**
             * @brief Destructor — delegates cleanup to @c SocketBase::closeSocket().
             *
             * @throws Nothing — marked @c noexcept.
             */
            ~Udp() noexcept;

            /// @brief Default constructor — socket is not initialized until @c init() is called.
            Udp() = default;

            /**
             * @brief Receives a datagram and captures the sender's address.
             *
             * Constructs a temporary @c Address sized for the current IP version,
             * then calls @c ::recvfrom(). The sender's address is written back into
             * the @c Address by the OS via @c getAddrRaw() / @c getSizeRaw().
             *
             * On Windows, @p buffer is cast to @c char* and @p length to @c int
             * to satisfy the Winsock API.
             *
             * @param buffer  Pointer to the caller-allocated buffer where the
             *                received bytes will be written. Must not be @c nullptr
             *                and must be at least @p length bytes.
             * @param length  Maximum number of bytes to read into @p buffer.
             *
             * @return On success, a @c Result<RecvFromResult> holding a tuple of
             *         the byte count received (@c ssize) and the sender's @c Address.
             * @return @c std::unexpected{Error::SocketNotInitialized} if @c init()
             *         has not been called.
             * @return @c std::unexpected with the IP type error if @c getIpType()
             *         fails.
             * @return @c std::unexpected{Error::ConnectionClosed} if @c ::recvfrom()
             *         returns @c 0.
             * @return @c std::unexpected with the platform error (from @c getError())
             *         if @c ::recvfrom() returns @c -1.
             *
             * @throws Nothing — marked @c noexcept.
             */
            Result<Net::RecvFromResult> receiveFrom(uint8_t* buffer, size_t length) noexcept;

            /**
             * @brief Sends a datagram to a specific remote address.
             *
             * Calls @c ::sendto() using the raw address and size from @p destination.
             * On Windows, @p data is cast to @c char* and @p size to @c int.
             *
             * @param data         Pointer to the buffer containing the bytes to send.
             *                     Must not be @c nullptr and must be at least @p size bytes.
             * @param size         Number of bytes to send from @p data.
             * @param destination  The remote @c Address to send the datagram to.
             *
             * @return On success, a @c Result<ssize> holding the number of bytes sent.
             * @return @c std::unexpected{Error::SocketNotInitialized} if @c init()
             *         has not been called.
             * @return @c std::unexpected{Error::ConnectionClosed} if @c ::sendto()
             *         returns @c 0.
             * @return @c std::unexpected with the platform error (from @c getError())
             *         if @c ::sendto() returns @c -1.
             *
             * @throws Nothing — marked @c noexcept.
             */
            Result<ssize> sendTo(const void* data, size_t size, const Net::Address& destination) noexcept;

            /**
             * @brief Returns @c SOCK_DGRAM — the socket type for UDP.
             *
             * @return @c SOCK_DGRAM.
             *
             * @throws Nothing — marked @c noexcept.
             */
            int socketType() const noexcept override { return SOCK_DGRAM; }

            /// @brief Deleted — socket ownership cannot be shared.
            Udp(const Udp&) = delete;
            /// @brief Deleted — socket ownership cannot be shared.
            Udp& operator=(const Udp&) = delete;

            /// @brief Default move constructor — transfers socket ownership.
            Udp(Udp&&) noexcept = default;
            /// @brief Default move assignment — transfers socket ownership.
            Udp& operator=(Udp&&) noexcept = default;
        };

        /**
         * @brief TCP socket server (@c SOCK_STREAM).
         *
         * Extends @c SocketBase with connection-oriented operations.
         * The typical server-side flow is:
         * @code
         * Tcp server;
         * server.init(IPType::V4);
         * server.bind("0.0.0.0", 8080);
         * server.listen();
         * auto client = server.accept();
         * @endcode
         *
         * Non-copyable. Movable via the default move constructor and assignment.
         */
        class Tcp : public SocketBase {
        public:

            /**
             * @brief Marks the socket as passive and ready to accept connections.
             *
             * Calls @c ::listen() with the given backlog. Must be called after
             * @c bind() and before @c accept().
             *
             * @param backlog  Maximum number of pending connections the OS will
             *                 queue before refusing new ones. Defaults to @c 10.
             *
             * @return @c Result<void> with no value on success.
             * @return @c std::unexpected{Error::SocketNotInitialized} if @c init()
             *         has not been called.
             * @return @c std::unexpected with the platform error (from @c getError())
             *         if @c ::listen() fails.
             *
             * @throws Nothing — marked @c noexcept.
             */
            Result<void> listen(int backlog = 10) const noexcept;

            /**
             * @brief Accepts the next pending connection and returns a @c Client.
             *
             * Calls @c ::accept(), which blocks until a client connects (unless the
             * socket is set to non-blocking). On success, wraps the accepted socket
             * and the peer's @c Address in a heap-allocated @c Client via
             * @c std::make_unique.
             *
             * On Windows, the accepted handle is a @c SOCKET; on POSIX it is an
             * @c int. Both paths check against their respective sentinel values
             * before returning an error.
             *
             * @return On success, a @c Result holding a @c std::unique_ptr<Client>
             *         for the newly connected peer.
             * @return @c std::unexpected{Error::SocketNotInitialized} if @c init()
             *         has not been called.
             * @return @c std::unexpected with the platform error (from @c getError())
             *         if @c ::accept() fails.
             *
             * @throws Nothing — marked @c noexcept.
             */
            Result<std::unique_ptr<Client>> accept() const noexcept;

          

            /**
             * @brief Closes the TCP socket explicitly.
             *
             * @return @c Result<void> with no value on success.
             * @return @c std::unexpected with the appropriate @c Error on failure.
             *
             * @throws Nothing — marked @c noexcept.
             */
            Result<void> close() const noexcept;

            /**
             * @brief Destructor — delegates cleanup to @c SocketBase::closeSocket().
             *
             * @throws Nothing — marked @c noexcept.
             */
            ~Tcp() noexcept;

            /**
             * @brief Returns @c SOCK_STREAM — the socket type for TCP.
             *
             * @return @c SOCK_STREAM.
             *
             * @throws Nothing — marked @c noexcept.
             */
            int socketType() const noexcept override { return SOCK_STREAM; }

            /// @brief Default constructor — socket is not initialized until @c init() is called.
            Tcp() = default;

            /// @brief Deleted — socket ownership cannot be shared.
            Tcp(const Tcp&) = delete;
            /// @brief Deleted — socket ownership cannot be shared.
            Tcp& operator=(const Tcp&) = delete;

            /// @brief Default move constructor — transfers socket ownership.
            Tcp(Tcp&&) noexcept = default;
            /// @brief Default move assignment — transfers socket ownership.
            Tcp& operator=(Tcp&&) noexcept = default;
        };

    } // namespace Servers
} // namespace Net
