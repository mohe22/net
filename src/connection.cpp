#include "../include/connection.hpp"
#include <cstdint>
#include <expected>
#include <span>
namespace Net {


    Connection::~Connection() noexcept{

        if (socket_ != invalidSocket) {
            platformClose(socket_);
        }
    }
    Connection::Connection(SocketHandle socket, const Address &address) noexcept
        : socket_{socket}, address_{address}
    {
    }

    Result<std::unique_ptr<Connection>> Connection::connect(const std::string &ip,uint16_t port,IPType ipType) noexcept {
        #ifdef _WIN32
                WSADATA wsaData;
                if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
                    return std::unexpected{Error::WSAStartupFailed};
        #endif

        auto addressResult = Address::from(ip, port);
        if (!addressResult)
            return std::unexpected{addressResult.error()};

        const Address &address = addressResult.value();

        auto familyResult = toAddressFamily(ipType);
        if (!familyResult)
            return std::unexpected{familyResult.error()};

        SocketHandle s = ::socket(familyResult.value(), SOCK_STREAM, 0);

        if (s == invalidSocket)
            return std::unexpected{Net::getError()};

        if (::connect(s, address.getAddrRaw(), address.getSize()) == -1)
        {
            platformClose(s);
            return std::unexpected{Net::getError()};
        }

        return std::make_unique<Connection>(s, address);
    }

    Result<ssize> Connection::receive(void *data, size_t size) noexcept
    {
        #ifdef _WIN32
                ssize result = ::recv(socket_, static_cast<char *>(data), static_cast<int>(size), 0);
        #else
                ssize result = ::recv(socket_, data, size, 0);
        #endif
            if (result == 0)
                return std::unexpected{Net::Error::ConnectionClosed};
            if (result == -1){
                auto err = getError();
                #ifndef _WIN32
                /*
                    https://www.man7.org/linux/man-pages/man3/errno.3.html
                    According to the above man page:
                    with the exception of EAGAIN and EWOULDBLOCK, which may be
                    the same. On Linux, these two have the same value on all
                    architectures.

                */
                if (err == Net::Error::WouldBlock)
                    return std::unexpected{
                        isBlocking() ? Net::Error::ConnectionTimeout : Net::Error::WouldBlock
                    };
                #endif

                // std::printf("recv() failed with error code %d\n", getLastError());
                return std::unexpected{err};
        }
        return result;
    }

    Result<ssize> Connection::send(const void *data, size_t size) noexcept {
        #ifdef _WIN32
                ssize result = ::send(socket_, static_cast<const char *>(data), static_cast<int>(size), 0);
        #else
                ssize result = ::send(socket_, data, size, 0);
        #endif
                if (result == 0)
                    return std::unexpected{Net::Error::ConnectionClosed};
                if (result == -1)
                {
                    auto err = Net::getError();
        #ifndef _WIN32
                    /*
                        https://www.man7.org/linux/man-pages/man3/errno.3.html
                        According to the above man page:
                        with the exception of EAGAIN and EWOULDBLOCK, which may be
                        the same. On Linux, these two have the same value on all
                        architectures.

                    */
                    if (err == Net::Error::WouldBlock)
                        return std::unexpected{
                            isBlocking() ? Net::Error::ConnectionTimeout : Net::Error::WouldBlock
                        };
        #endif
                    return std::unexpected{err};
                }
                return result;
    }


    Result<ssize> Connection::sendAll(std::span<const uint8_t> buffer, size_t totalBytes) noexcept {
        if(buffer.size() < totalBytes){
            return std::unexpected<Net::Error>{Net::Error::BufferTooSmall};
        }

        ssize offset = 0;
        while(offset < totalBytes){
            auto result = send(
                buffer.subspan(offset).data(),
                totalBytes - offset
            );
            if(!result) return std::unexpected{result.error()};
            offset += result.value();
        }
        return offset;
    }
    Result<ssize> Connection::receiveAll(std::span<uint8_t> data,size_t totalBytes) noexcept {
        if(data.size() < totalBytes){
            return std::unexpected<Net::Error>{Net::Error::BufferTooSmall};
        }

        ssize offset = 0;
        while(offset < totalBytes){
            auto result = receive(
                data.subspan(offset).data(),
                totalBytes - offset
            );
            if(!result) return std::unexpected{result.error()};
            offset += result.value();
        }
        return offset;
    }

    Result<void> Connection::close()  noexcept {
        if (socket_ == invalidSocket)
            return std::unexpected{Net::getError()};
        platformClose(socket_);
        socket_ = invalidSocket;
        return {};
    }

}
