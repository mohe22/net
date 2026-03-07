#include "../include/server.hpp"



namespace Net::Servers {

    Result<void> SocketBase::init(IPType ipType) noexcept {
        // Windows must initialize Winsock before any socket calls
        #ifdef _WIN32
            if (wsaInitialized_)
                return std::unexpected{Error::AlreadyConnected};
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
                return std::unexpected{Error::WSAStartupFailed};
            wsaInitialized_ = true;
        #endif
           return toAddressFamily(ipType)
            .and_then([&](int addressFamily) -> Result<int> {
                  return  socket(addressFamily, socketType(), 0);
              })
            .and_then([&](int handle) -> Result<void> {
                       if (handle == invalidSocket)
                           return std::unexpected{Net::getError()}; // OS tells us exactly why socket() failed
                       socket_ = handle;
                       return {};
            });
    };
    Result<void> SocketBase::bind(const std::string& ip, uint16_t port) noexcept{
        if (socket_ == invalidSocket)
               return std::unexpected{Error::SocketNotInitialized};
        return Net::Address::from(ip,port).and_then([&](const Address& address)->Result<void> {
            if(::bind(socket_,  address.getAddrRaw(),address.getSize())== -1){
                return std::unexpected{Net::getError()};
            }
            address_ = address;
            return {};
        });

    };


    Result<void> Tcp::listen(int backlog) const noexcept{
        if (!isValidSocket())
               return std::unexpected{Error::SocketNotInitialized};
        if (::listen(getSocket(), backlog) == -1)
            return std::unexpected{Net::getError()};
        return {};
    };


    Result<std::unique_ptr<Connection>> Tcp::accept() const noexcept {
        if (!isValidSocket())
            return std::unexpected{Error::SocketNotInitialized};


        sockaddr_storage clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        #ifdef _WIN32
            SocketHandle clientSocket = ::accept(getSocket(), reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLength);
            if (clientSocket == INVALID_SOCKET)
                return std::unexpected{getError()};
        #else
        SocketHandle clientSocket = ::accept(getSocket(), reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressLength);
        if (clientSocket == -1)
            return std::unexpected{getError()};
        #endif
            Result<Net::Address> clientAddressResult = Net::Address::from(clientAddress);
            if(!clientAddressResult)
                return std::unexpected{clientAddressResult.error()};

            return std::make_unique<Net::Connection>(clientSocket, clientAddressResult.value());
    }


    Tcp::~Tcp() noexcept{
        closeSocket();
    };

    Udp::~Udp() noexcept{
        closeSocket();
    }

    Result<ssize> Udp::sendTo(const void* data, size_t size, const Net::Address& destination) noexcept {
        if (!isValidSocket())
            return std::unexpected{Error::SocketNotInitialized};

        #ifdef _WIN32
        ssize sentBytes = ::sendto(
                    getSocket(),
                    (char*)data,
                    size,
                    0,
                    destination.getAddrRaw(),
                    destination.getSize()
            );
        #else
            ssize sentBytes = ::sendto(
                getSocket(),
                data,
                size,
                0,
                destination.getAddrRaw(),
                destination.getSize()
            );
        #endif
        if(sentBytes == 0 ) return std::unexpected{Net::Error::ConnectionClosed};
        if (sentBytes == -1)
            return std::unexpected{getError()};
        return sentBytes;
    }

    Result<Net::RecvFromResult> Udp::receiveFrom(uint8_t* buffer, size_t length) noexcept {
        if(!isValidSocket())
            return std::unexpected{Error::SocketNotInitialized};

        Result<IPType> result = getIpType();
        if(!result)
            return std::unexpected{result.error()};

        Address receiver(result.value());

        #ifdef _WIN32
            ssize recvBytes =::recvfrom(
                getSocket(),
                (char*)buffer,
                length,
                0,
                receiver.getAddrRaw(),
                receiver.getSizeRaw()
            );
        #else
            ssize recvBytes = ::recvfrom(
                getSocket(),
                buffer,
                length,
                0,
                receiver.getAddrRaw(),
                receiver.getSizeRaw()
            );
        #endif
        if(recvBytes == 0 ) return std::unexpected{Net::Error::ConnectionClosed};
        if (recvBytes == -1)
            return std::unexpected{getError()};

        return std::make_tuple(recvBytes, receiver);
    }



};
