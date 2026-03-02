#include "../include/client.hpp"
namespace Net {


    Client::~Client() noexcept{

        if (socket_ != invalidSocket) {
            platformClose(socket_);
        }
        std::println("[DEBUG] {}:{} Disconnected",address_.getIp().value_or("Unkown"),address_.getPort().value_or(0));
    }
    Client::Client(SocketHandle socket, const Address& address) noexcept
        : socket_{socket}, address_{std::move(address)} {
            std::println("[DEBUG] client {}:{} established",address_.getIp().value_or("Unkown"),address_.getPort().value_or(0));
    }

    Result<ssize> Client::send(const void* data, size_t size) {
        #ifdef _WIN32
            ssize result = ::send(socket_, static_cast<const char*>(data), static_cast<int>(size), 0);
        #else
            ssize result = ::send(socket_, data, size, 0);
        #endif
            if(result == 0 ) return std::unexpected{Net::Error::ConnectionClosed};
            if (result == -1)
                return std::unexpected{Net::getError()};
        return result;
    }
    Result<ssize> Client::receive(void* data, size_t size) {
        #ifdef _WIN32
            ssize result = ::recv(socket_, static_cast<char*>(data), static_cast<int>(size), 0);
        #else
            ssize result = ::recv(socket_, data, size, 0);
        #endif
            if(result == 0 ) return std::unexpected{Net::Error::ConnectionClosed};
            if (result == -1)
                return std::unexpected{Net::getError()};
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
