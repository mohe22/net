#include "../include/client.hpp"
namespace Net {


    Client::~Client() noexcept{

        if (socket_ != invalidSocket) {
            platformClose(socket_);
        }
    }
    Client::Client(SocketHandle socket, const Address &address) noexcept
        : socket_{socket}, address_{address}
    {
    }

    Result<std::unique_ptr<Client>> Client::connect(const std::string &ip,uint16_t port,IPType ipType) noexcept {
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

        return std::make_unique<Client>(s, address);
    }

    Result<ssize> Client::receive(void *data, size_t size)
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

                    TODO: This approach always remaps WouldBlock to ConnectionTimeout.
                    If non-blocking sockets are added, add `isBlocking_`
                    to avoid returning wrong error WouldBlock as ConnectionTimeout.
                */
                if (err == Net::Error::WouldBlock)
                    return std::unexpected{Net::Error::ConnectionTimeout};
                #endif

                // std::printf("recv() failed with error code %d\n", getLastError());
                return std::unexpected{err};
        }
        return result;
    }

    Result<ssize> Client::send(const void *data, size_t size){
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


                        TODO: This approach always remaps WouldBlock to ConnectionTimeout.
                        If non-blocking sockets are added, add `isBlocking_`
                        to avoid returning wrong error WouldBlock as ConnectionTimeout.
                    */
                    if (err == Net::Error::WouldBlock)
                        return std::unexpected{Net::Error::ConnectionTimeout};
        #endif
                    return std::unexpected{err};
                }
                return result;
    }

    Result<void> Client::close() {
        if (socket_ == invalidSocket)
            return std::unexpected{Net::getError()};
        platformClose(socket_);
        socket_ = invalidSocket;
        return {};
    }

}
