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
