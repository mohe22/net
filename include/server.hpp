#pragma once
#include "platform.hpp"
#include "types.hpp"
#include "connection.hpp"
#include "socketOptions.hpp"

#include <chrono>
#include <atomic>

namespace Net {
    namespace Servers {

        /**
         * @brief Abstract base class providing shared socket lifecycle management.
         *
         * Owns a single SocketHandle and its associated Address. Handles
         * platform-level initialization (Winsock on Windows), binding, and cleanup.
         * Concrete subclasses (Tcp, Udp) provide the socket type via the
         * pure virtual socketType() and extend with protocol-specific operations.
         *
         * Lifecycle:
         *   init() → bind() → [listen()] → pause()/resume() → stop()/closeSocket()
         *
         * pause() / resume() — temporarily halt and continue operations.
         * stop()             — permanent exit, requires full re-init() to reuse.
         * closeSocket()      — closes the fd and automatically calls stop().
         */
        class SocketBase : public Net::SocketOptions {
        public:
            SocketBase(const SocketBase&)            = delete;
            SocketBase& operator=(const SocketBase&) = delete;

            SocketBase(SocketBase&& other) noexcept
                : socket_ (other.socket_)
                , address_(std::move(other.address_))
                , paused_ {other.paused_.load()}
                , stopped_{other.stopped_.load()}
            {
                other.socket_ = invalidSocket;
                other.paused_.store(false);
                other.stopped_.store(true); // moved-from is considered stopped
            }

            SocketBase& operator=(SocketBase&& other) noexcept {
                closeSocket(); // release our current socket first
                socket_  = other.socket_;
                address_ = std::move(other.address_);
                paused_.store(other.paused_.load());
                stopped_.store(other.stopped_.load());
                other.socket_ = invalidSocket;
                other.paused_.store(false);
                other.stopped_.store(true);
                return *this;
            }

            SocketBase() noexcept = default;

            ~SocketBase() noexcept {
                closeSocket();
                #ifdef _WIN32
                if (wsaInitialized_)
                    ::WSACleanup();
                #endif
            }


            /**
             * @brief Initializes the socket for the given IP version.
             * @param ipType IPv4 or IPv6.
             * @return Result<void> on success, error on failure.
             */
            Result<void> init(IPType ipType) noexcept;

            /**
             * @brief Binds the socket to a local IP address and port.
             * @param ip   Bind address (e.g. "0.0.0.0" or "::").
             * @param port Local port in host byte order.
             * @return Result<void> on success, error on failure.
             */
            Result<void> bind(const std::string& ip, uint16_t port) noexcept;

            /**
             * @brief Closes the socket fd and marks the server as stopped.
             *
             * Safe to call multiple times — no-op if already closed.
             * Automatically calls stop() so anything waiting on the stopped_
             * flag is unblocked.
             */
            void closeSocket() noexcept {
                stop();
            }


            /**
             * @brief Temporarily halts operations.
             *
             * Sets paused_ = true. The accept loop or any caller using
             * isPaused() should stop processing until resume() is called.
             * Does NOT close the socket — the server is still bound.
             */
            void pause() noexcept {
                paused_.store(true);
                paused_.notify_all();
            }

            /**
             * @brief Resumes operations after a pause().
             *
             * Sets paused_ = false and notifies any waiting threads.
             */
            void resume() noexcept {
                paused_.store(false);
                paused_.notify_all();
            }

            /**
             * @brief Returns true if the server is currently paused.
             */
            bool isPaused() const noexcept { return paused_.load(); }


            /**
             * @brief Signals a permanent stop.
             *
             * Sets stopped_ = true. Also clears paused_ and notifies all
             * waiters so that anything blocked in a pause() check can
             * observe the stopped_ flag and exit cleanly.
             *
             * After stop(), a full init() is required before reuse.
             */
            void stop() noexcept {
                stopped_.store(true);
                paused_.store(false);
                paused_.notify_all();
                if (socket_ != invalidSocket) {
                    platformClose(socket_);
                    socket_ = invalidSocket;
                }
            }

            /**
             * @brief Blocks until resume() or stop() is called.
             *
             * If paused, the calling thread blocks without spinning.
             * Returns immediately if not paused or if stopped.
             */
            void waitUntilRunning() const noexcept {
                while (paused_.load() && !stopped_.load())
                    paused_.wait(true);
            }

            /**
             * @brief Returns true if stop() or closeSocket() has been called.
             */
            bool isStopped() const noexcept { return stopped_.load(); }


            SocketHandle       getSocket()    const noexcept override { return socket_; }
            const Address&     getAddress()   const noexcept          { return address_; }
            const Result<IPType> getIpType()  const noexcept          { return address_.getIpType(); }
            bool               isValidSocket() const noexcept         { return socket_ != invalidSocket; }

        protected:
            /** @brief Returns the socket type (SOCK_STREAM or SOCK_DGRAM). */
            virtual int socketType() const noexcept = 0;

        private:
            Result<void> setOption(SocketOption option, const void* optval, int optlen = sizeof(int)) const noexcept;
            Result<void> setTimeoutOption(SocketOption option, std::chrono::milliseconds timeout) const noexcept;

            SocketHandle       socket_{invalidSocket}; ///< Underlying platform socket handle
            Address            address_{};             ///< Local bound address
            std::atomic<bool>  paused_{false};         ///< true = temporarily halted
            std::atomic<bool>  stopped_{false};        ///< true = permanent stop, needs re-init

            #ifdef _WIN32
            bool wsaInitialized_{false};
            #endif
        };


        /**
         * @brief UDP socket server (SOCK_DGRAM).
         *
         * Connectionless datagram operations. Each sendTo()/receiveFrom()
         * carries its own address — no accept() step required.
         */
        class Udp : public SocketBase {
        public:
            ~Udp() noexcept;
            Udp()                        = default;
            Udp(Udp&&) noexcept          = default;
            Udp& operator=(Udp&&)noexcept = default;

            /**
             * @brief Receives a datagram and captures the sender's address.
             */
            Result<Net::RecvFromResult> receiveFrom(uint8_t* buffer, size_t length) noexcept;

            /**
             * @brief Sends a datagram to a specific remote address.
             */
            Result<ssize> sendTo(const void* data, size_t size, const Net::Address& destination) const noexcept;

            int socketType() const noexcept override { return SOCK_DGRAM; }
        };


        /**
         * @brief TCP socket server (SOCK_STREAM).
         *
         * Connection-oriented. Typical server flow:
         * @code
         *   Tcp server;
         *   server.init(IPType::V4);
         *   server.bind("0.0.0.0", 8080);
         *   server.listen();
         *   auto client = server.accept();
         * @endcode
         */
        class Tcp : public SocketBase {
        public:
            ~Tcp() noexcept;
            Tcp() noexcept                = default;
            Tcp(Tcp&&) noexcept           = default;
            Tcp& operator=(Tcp&&) noexcept = default;

            /**
             * @brief Marks the socket as passive and ready to accept connections.
             * @param backlog Max pending connections queued by the OS. Default 10.
             */
            Result<void> listen(int backlog = 10) const noexcept;

            /**
             * @brief Accepts the next pending connection.
             * @return unique_ptr<Connection> wrapping the accepted client.
             */
            Result<std::unique_ptr<Connection>> accept() const noexcept;

            /**
             * @brief Explicitly closes the TCP socket.
             */
            Result<void> close() const noexcept;

            int socketType() const noexcept override { return SOCK_STREAM; }
        };

    } // namespace Servers
} // namespace Net
