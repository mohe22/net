#pragma once
#include "platform.hpp"
#include <print>
#include <string_view>


namespace Net {
    enum class Error : uint8_t {
        // -- General --
        Ok                    = 0,
        UnknownError          = 1,

        // -- Initialization --
        WSAStartupFailed      = 2,  // Windows Winsock init failed
        SocketCreationFailed  = 3,  // socket() returned invalid handle

        // -- Address / IP --
        InvalidIP             = 4,  // malformed IP string
        InvalidPort           = 5,  // port out of range (0 or >65535)
        InvalidAddressFamily  = 6,  // unknown IPType
        InvalidSocketType     = 7,  // unknown Protocol
        InvalidProtocol = 30, //
        // -- Bind --
        BindFailed            = 8,  // bind() failed
        AddressAlreadyInUse   = 9,  // port already occupied (EADDRINUSE)
        AddressNotAvailable   = 10, // IP not available on this machine

        // -- Listen --
        ListenFailed          = 11, // listen() failed
        NotBound              = 12, // listen() called before bind()

        // -- Accept --
        AcceptFailed          = 13, // accept() failed
        NotListening          = 14, // accept() called before listen()

        // -- Connect --
        ConnectFailed         = 15, // connect() failed
        ConnectionRefused     = 16, // remote refused the connection
        ConnectionTimeout     = 17, // connect timed out
        AlreadyConnected      = 18, // socket already connected

        // -- Send --
        SendFailed            = 19, // send() failed
        ConnectionReset       = 20, // remote forcibly closed connection
        BrokenPipe            = 21, // writing to closed socket

        // -- Receive --
        ReceiveFailed         = 22, // recv() failed
        ConnectionClosed      = 23, // remote closed connection gracefully

        // -- Close --
        CloseFailed           = 24, // close()/closesocket() failed

        // -- Socket State --
        SocketNotInitialized  = 25, // operation called before init()
        SocketAlreadyClosed   = 26, // operation called after close()
        WouldBlock            = 27, // non-blocking socket has no data yet

        // -- Raw Socket --
        RawSocketNotPermitted   = 29, // no privileges for raw socket (needs root/admin)


    };

    inline Error getError() noexcept {
        const int code = getLastError();
        std::println("[Debug] error code is {}", code);

    #ifdef _WIN32
        switch (code) {
            // --- Address / Bind ---
            case WSAEADDRINUSE:      return Error::AddressAlreadyInUse;
            case WSAEADDRNOTAVAIL:   return Error::AddressNotAvailable;
            case WSAEFAULT:          return Error::InvalidIP;
            case WSAEAFNOSUPPORT:    return Error::InvalidAddressFamily;
            case WSAEPFNOSUPPORT:    return Error::InvalidAddressFamily;

            // --- Connect ---
            case WSAECONNREFUSED:    return Error::ConnectionRefused;
            case WSAETIMEDOUT:       return Error::ConnectionTimeout;
            case WSAEISCONN:         return Error::AlreadyConnected;
            case WSAENOTCONN:        return Error::ConnectionClosed;

            // --- Send / Receive ---
            case WSAECONNRESET:      return Error::ConnectionReset;
            case WSAESHUTDOWN:       return Error::ConnectionClosed;
            case WSAEMSGSIZE:        return Error::SendFailed;
            case WSAENOBUFS:         return Error::SendFailed;

            // --- Non-blocking ---
            case WSAEWOULDBLOCK:     return Error::WouldBlock;
            case WSAEINPROGRESS:     return Error::WouldBlock;
            case WSAEALREADY:        return Error::WouldBlock;

            // --- Socket state ---
            case WSAENOTSOCK:        return Error::SocketNotInitialized;
            case WSAEINVAL:          return Error::InvalidSocketType;
            case WSAEOPNOTSUPP:      return Error::InvalidSocketType;


            // --- Permissions ---
            case WSAEACCES:          return Error::RawSocketNotPermitted;

            // --- System ---
            case WSAENETDOWN:
            case WSAENETUNREACH:
            case WSAEHOSTUNREACH:    return Error::ConnectionClosed;

            default:
                return Error::UnknownError;
        }

    #else
        switch (code) {
            // --- Address / Bind ---
            case EADDRINUSE:         return Error::AddressAlreadyInUse;
            case EADDRNOTAVAIL:      return Error::AddressNotAvailable;
            case EFAULT:             return Error::InvalidIP;
            case EAFNOSUPPORT:       return Error::InvalidAddressFamily;
            case EPFNOSUPPORT:       return Error::InvalidAddressFamily;

            // --- Connect ---
            case ECONNREFUSED:       return Error::ConnectionRefused;
            case ETIMEDOUT:          return Error::ConnectionTimeout;
            case EISCONN:            return Error::AlreadyConnected;
            case ENOTCONN:           return Error::ConnectionClosed;

            // --- Send / Receive ---
            case ECONNRESET:         return Error::ConnectionReset;
            case EPIPE:              return Error::BrokenPipe;
            case EMSGSIZE:           return Error::SendFailed;
            case ENOBUFS:            return Error::SendFailed;

            // --- Non-blocking ---
            case EWOULDBLOCK:
            case EAGAIN:             return Error::WouldBlock;
            case EINPROGRESS:        return Error::WouldBlock;
            case EALREADY:           return Error::WouldBlock;

            // --- Socket state ---
            case ENOTSOCK:           return Error::SocketNotInitialized;
            case EINVAL:             return Error::InvalidSocketType;
            case EOPNOTSUPP:         return Error::InvalidSocketType;

            // --- Permissions ---
            case EACCES:
            case EPERM:              return Error::RawSocketNotPermitted;

            // --- Network ---
            case ENETDOWN:
            case ENETUNREACH:
            case EHOSTUNREACH:       return Error::ConnectionFailed;

            default:
                return Error::UnknownError;
        }
    #endif
    }
    inline std::string_view toErrorString(Error error) noexcept {
        switch (error) {
            // -- General --
            case Error::Ok:                     return "Ok";
            case Error::UnknownError:           return "Unknown error";

            // -- Initialization --
            case Error::WSAStartupFailed:       return "Windows Winsock initialization failed";
            case Error::SocketCreationFailed:   return "Failed to create socket";

            // -- Address / IP --
            case Error::InvalidIP:              return "Invalid IP address format";
            case Error::InvalidPort:            return "Port out of range (0 or >65535)";
            case Error::InvalidAddressFamily:   return "Unknown IP type or incompatible address family";
            case Error::InvalidSocketType:      return "Unknown protocol type";
            case Error::InvalidProtocol:        return "Invalid protocol for this operation";

            // -- Bind --
            case Error::BindFailed:             return "Failed to bind socket";
            case Error::AddressAlreadyInUse:    return "Address already in use (port occupied)";
            case Error::AddressNotAvailable:    return "Address not available on this machine";

            // -- Listen --
            case Error::ListenFailed:           return "Failed to listen on socket";
            case Error::NotBound:               return "Cannot listen before binding";

            // -- Accept --
            case Error::AcceptFailed:           return "Failed to accept connection";
            case Error::NotListening:           return "Cannot accept before listening";

            // -- Connect --
            case Error::ConnectFailed:          return "Failed to connect";
            case Error::ConnectionRefused:      return "Connection refused by remote";
            case Error::ConnectionTimeout:      return "Connection timed out";
            case Error::AlreadyConnected:       return "Socket is already connected";

            // -- Send --
            case Error::SendFailed:             return "Failed to send data";
            case Error::ConnectionReset:        return "Connection forcibly closed by remote";
            case Error::BrokenPipe:             return "Cannot write to a closed socket";

            // -- Receive --
            case Error::ReceiveFailed:          return "Failed to receive data";
            case Error::ConnectionClosed:       return "Connection closed gracefully by remote";

            // -- Close --
            case Error::CloseFailed:            return "Failed to close socket";

            // -- Socket State --
            case Error::SocketNotInitialized:   return "Socket not initialized, call init() first";
            case Error::SocketAlreadyClosed:    return "Socket is already closed";
            case Error::WouldBlock:             return "Non-blocking socket has no data yet";

            // -- Raw Socket --
            case Error::RawSocketNotPermitted:   return "Raw socket requires root/admin privileges";
        }
        return "Unrecognized error";
    }
} // namespace Net;
