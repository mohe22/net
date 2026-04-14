#pragma once
#include "platform.hpp"
#include <string_view>
#include <cstdint>

namespace Net {

enum class Error : uint8_t {

    // ── General ───────────────────────────────────────────────────────────────
    Ok                      = 0,
    UnknownError            = 1,

    // ── Initialization ────────────────────────────────────────────────────────
    WSAStartupFailed        = 2,   ///< Windows only — WSAStartup() failed.
    SocketCreationFailed    = 3,   ///< socket() returned an invalid handle.

    // ── Address / IP ──────────────────────────────────────────────────────────
    InvalidIP               = 4,   ///< IP string could not be parsed by inet_pton().
    InvalidPort             = 5,   ///< Port was 0 or out of valid range.
    InvalidAddressFamily    = 6,   ///< Unrecognized ss_family / sin_family value.
    InvalidSocketType       = 7,   ///< Unknown or unsupported SOCK_* type.
    InvalidProtocol         = 8,   ///< Protocol incompatible with the requested operation.
    InvalidArgument         = 9,   ///< A caller-supplied argument is invalid (e.g. size == 0).

    // ── Bind ──────────────────────────────────────────────────────────────────
    BindFailed              = 10,  ///< bind() failed for a reason not covered below.
    AddressAlreadyInUse     = 11,  ///< Port already occupied (EADDRINUSE).
    AddressNotAvailable     = 12,  ///< Requested IP not assigned to any local interface.

    // ── Listen ────────────────────────────────────────────────────────────────
    ListenFailed            = 13,  ///< listen() failed.
    NotBound                = 14,  ///< listen() called before bind().

    // ── Accept ────────────────────────────────────────────────────────────────
    AcceptFailed            = 15,  ///< accept() failed.
    NotListening            = 16,  ///< accept() called before listen().

    // ── Connect ───────────────────────────────────────────────────────────────
    ConnectFailed           = 17,  ///< connect() failed for a reason not covered below.
    ConnectionRefused       = 18,  ///< Remote host actively refused the connection.
    ConnectionTimeout       = 19,  ///< Connection attempt timed out.
    AlreadyConnected        = 20,  ///< connect() called on an already-connected socket.

    // ── Send ──────────────────────────────────────────────────────────────────
    SendFailed              = 21,  ///< send() failed for a reason not covered below.
    ConnectionReset         = 22,  ///< Remote peer forcibly closed the connection.
    BrokenPipe              = 23,  ///< POSIX: write to a socket whose read end is closed.
    MessageTooLarge         = 24,  ///< Datagram exceeds max UDP payload (EMSGSIZE).

    // ── Receive ───────────────────────────────────────────────────────────────
    ReceiveFailed           = 25,  ///< recv() failed for a reason not covered below.
    ConnectionClosed        = 26,  ///< Remote peer closed the connection gracefully.
    BufferTooSmall          = 27,  ///< Buffer too small to hold the requested data.

    // ── Close ─────────────────────────────────────────────────────────────────
    CloseFailed             = 28,  ///< close() / closesocket() failed.

    // ── Socket State ──────────────────────────────────────────────────────────
    InvalidSocket           = 29,  ///< The socket handle is invalid.
    SocketNotInitialized    = 30,  ///< Operation attempted before socket was initialized.
    SocketAlreadyClosed     = 31,  ///< Operation attempted after socket was closed.
    WouldBlock              = 32,  ///< Non-blocking socket has no data ready (EAGAIN).
    SocketOptionFailed      = 33,  ///< Setting a socket option failed.

    // ── Raw Socket ────────────────────────────────────────────────────────────
    RawSocketNotPermitted   = 34,  ///< Raw socket requires root / Administrator privileges.

    // ── Epoll ─────────────────────────────────────────────────────────────────
    EpollCreationFailed     = 35,  ///< Creating an epoll instance failed.
    EpollInvalidFlags       = 36,  ///< Invalid flags passed to epoll.
    EpollLimitReached       = 37,  ///< Max file descriptors for epoll reached.
    EpollEventFailed        = 38,  ///< Adding, modifying, or removing an epoll event failed.
    EpollInsufficientMemory = 39,  ///< Insufficient memory for an epoll event.
    EpollInvalidDescriptor  = 40,  ///< File descriptor passed to epoll is invalid.
};

inline Error getError() noexcept {
    const int code = getLastError();
#ifdef _WIN32
    switch (code) {
        case WSAEADDRINUSE:      return Error::AddressAlreadyInUse;
        case WSAEADDRNOTAVAIL:   return Error::AddressNotAvailable;
        case WSAEFAULT:          return Error::InvalidIP;
        case WSAEAFNOSUPPORT:    return Error::InvalidAddressFamily;
        case WSAEPFNOSUPPORT:    return Error::InvalidAddressFamily;
        case WSAECONNREFUSED:    return Error::ConnectionRefused;
        case WSAETIMEDOUT:       return Error::ConnectionTimeout;
        case WSAEISCONN:         return Error::AlreadyConnected;
        case WSAENOTCONN:        return Error::ConnectionClosed;
        case WSAECONNRESET:      return Error::ConnectionReset;
        case WSAESHUTDOWN:       return Error::ConnectionClosed;
        case WSAEMSGSIZE:        return Error::MessageTooLarge;
        case WSAENOBUFS:         return Error::SendFailed;
        case WSAEWOULDBLOCK:     return Error::WouldBlock;
        case WSAEINPROGRESS:     return Error::WouldBlock;
        case WSAEALREADY:        return Error::WouldBlock;
        case WSAENOTSOCK:        return Error::SocketNotInitialized;
        case WSAEINVAL:          return Error::InvalidSocketType;
        case WSAEOPNOTSUPP:      return Error::InvalidSocketType;
        case WSAEACCES:          return Error::RawSocketNotPermitted;
        case WSAENETDOWN:
        case WSAENETUNREACH:
        case WSAEHOSTUNREACH:    return Error::ConnectFailed;
        default:                 return Error::UnknownError;
    }
#else
    switch (code) {
        case EADDRINUSE:         return Error::AddressAlreadyInUse;
        case EADDRNOTAVAIL:      return Error::AddressNotAvailable;
        case EFAULT:             return Error::InvalidIP;
        case EAFNOSUPPORT:       return Error::InvalidAddressFamily;
        case EPFNOSUPPORT:       return Error::InvalidAddressFamily;
        case ECONNREFUSED:       return Error::ConnectionRefused;
        case ETIMEDOUT:          return Error::ConnectionTimeout;
        case EISCONN:            return Error::AlreadyConnected;
        case ENOTCONN:           return Error::ConnectionClosed;
        case ECONNRESET:         return Error::ConnectionReset;
        case EPIPE:              return Error::BrokenPipe;
        case EMSGSIZE:           return Error::MessageTooLarge;
        case ENOBUFS:            return Error::SendFailed;
        case EWOULDBLOCK:        return Error::WouldBlock;
        case EINPROGRESS:        return Error::WouldBlock;
        case EALREADY:           return Error::WouldBlock;
        case ENOTSOCK:           return Error::SocketNotInitialized;
        case EINVAL:             return Error::InvalidSocketType;
        case EOPNOTSUPP:         return Error::InvalidSocketType;
        case EACCES:
        case EPERM:              return Error::RawSocketNotPermitted;
        case EMFILE:             return Error::EpollLimitReached;
        case ENFILE:             return Error::EpollLimitReached;
        case ENOSPC:             return Error::EpollLimitReached;
        case ENOMEM:             return Error::EpollInsufficientMemory;
        case EBADF:              return Error::EpollEventFailed;
        case ENOENT:             return Error::EpollEventFailed;
        case EEXIST:             return Error::EpollEventFailed;
        case ENETDOWN:
        case ENETUNREACH:
        case EHOSTUNREACH:       return Error::ConnectFailed;
        default:                 return Error::UnknownError;
    }
#endif
}

inline std::string_view toErrorString(Error error) noexcept {
    switch (error) {
        case Error::Ok:                      return "Ok";
        case Error::UnknownError:            return "Unknown error";
        case Error::WSAStartupFailed:        return "Windows Winsock initialization failed";
        case Error::SocketCreationFailed:    return "Failed to create socket";
        case Error::InvalidSocket:           return "Invalid socket handle";
        case Error::SocketOptionFailed:      return "Setting a socket option failed";
        case Error::InvalidIP:               return "Invalid IP address format";
        case Error::InvalidPort:             return "Port out of range (0 or >65535)";
        case Error::InvalidAddressFamily:    return "Unknown IP type or incompatible address family";
        case Error::InvalidSocketType:       return "Unknown protocol type";
        case Error::InvalidProtocol:         return "Invalid protocol for this operation";
        case Error::InvalidArgument:         return "Invalid argument supplied by caller";
        case Error::BindFailed:              return "Failed to bind socket";
        case Error::AddressAlreadyInUse:     return "Address already in use (port occupied)";
        case Error::AddressNotAvailable:     return "Address not available on this machine";
        case Error::ListenFailed:            return "Failed to listen on socket";
        case Error::NotBound:                return "Cannot listen before binding";
        case Error::AcceptFailed:            return "Failed to accept connection";
        case Error::NotListening:            return "Cannot accept before listening";
        case Error::BufferTooSmall:          return "Buffer too small to hold data";
        case Error::ConnectFailed:           return "Failed to connect";
        case Error::ConnectionRefused:       return "Connection refused by remote";
        case Error::ConnectionTimeout:       return "Connection timed out";
        case Error::AlreadyConnected:        return "Socket is already connected";
        case Error::SendFailed:              return "Failed to send data";
        case Error::MessageTooLarge:         return "Datagram too large (max UDP payload is 65507 bytes)";
        case Error::ConnectionReset:         return "Connection forcibly closed by remote";
        case Error::BrokenPipe:              return "Cannot write to a closed socket";
        case Error::ReceiveFailed:           return "Failed to receive data";
        case Error::ConnectionClosed:        return "Connection closed gracefully by remote";
        case Error::CloseFailed:             return "Failed to close socket";
        case Error::SocketNotInitialized:    return "Socket not initialized, call init() first";
        case Error::SocketAlreadyClosed:     return "Socket is already closed";
        case Error::WouldBlock:              return "Non-blocking socket has no data yet (EAGAIN)";
        case Error::RawSocketNotPermitted:   return "Raw socket requires root/admin privileges";
        case Error::EpollCreationFailed:     return "Failed to create epoll instance";
        case Error::EpollInvalidFlags:       return "Invalid flags passed to epoll";
        case Error::EpollLimitReached:       return "Epoll file descriptor limit reached";
        case Error::EpollEventFailed:        return "Failed to add, modify, or remove epoll event";
        case Error::EpollInsufficientMemory: return "Insufficient memory for epoll event";
        case Error::EpollInvalidDescriptor:  return "Invalid file descriptor passed to epoll — object must inherit from Net::Poll::Descriptor";
    }
    return "Unrecognized error";
}

} // namespace Net
