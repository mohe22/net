#pragma once
#include "types.hpp"
#include "client.hpp"

#include <cstdint>
#include<memory>
#include <winnt.h>

// TODO clean up the WSA
namespace Net {
    namespace Servers {
        class SocketBase {
            public:
                Result<void> init(IPType ipType) noexcept;
                Result<void> bind(const std::string& ip, uint16_t port) noexcept;
                ~SocketBase() noexcept {

                    closeSocket();
                    #ifdef _WIN32
                        if (wsaInitialized_)
                            ::WSACleanup();
                    #endif
                }
                const SocketHandle getSocket() const noexcept { return socket_; }
                const Address& getAddress() const noexcept { return address_; }
                const Result<IPType> getIpType() const noexcept { return address_.getIpType(); }

                // Helpers
                bool isValidSocket() const noexcept {
                    return socket_ != invaliedSocket;
                };
                void closeSocket() noexcept {
                    if (isValidSocket()){
                        platformClose(socket_);
                    }
                    socket_ = invaliedSocket;
                }

            protected:
                virtual int socketType() const noexcept = 0;
            private:
                SocketHandle socket_{invaliedSocket};
                Address address_{};
                #ifdef _WIN32
                    bool wsaInitialized_{false};
                #endif

         };


        class Udp: public SocketBase {


            public:
                ~Udp() noexcept ;
                Udp() = default;


                Result<Net::RecvFromResult> receiveFrom(uint8_t* buffer,size_t length) noexcept;

                Result<ssize> sendTo(const void* data, size_t size,const Net::Address& destination) noexcept;
                int socketType() const noexcept override { return SOCK_DGRAM; }

                // Non-copyable
                Udp(const Udp&) = delete;
                Udp& operator=(const Udp&) = delete;

                // Movable
                Udp(Udp&&) noexcept    = default;
                Udp& operator=(Udp&&) noexcept = default;
        };

        class Tcp: public SocketBase{
            public:
                Result<void> listen(int backlog = 10) const noexcept;
                Result<std::unique_ptr<Client>> accept() const noexcept;
                Result<void> connect(const std::string &ip, IPType type,uint16_t port) const noexcept;
                Result<void> close() const noexcept;
                ~Tcp() noexcept;
                int socketType() const noexcept override { return SOCK_STREAM; }
                Tcp() = default;
                // Non-copyable
                Tcp(const Tcp&) = delete;
                Tcp& operator=(const Tcp&) = delete;
                // Movable
                Tcp(Tcp&&) noexcept    = default;
                Tcp& operator=(Tcp&&) noexcept = default;

        };

     }// namespace Servers
} // namespace Net
