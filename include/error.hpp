#pragma once
#include "platform.hpp"
#include <string_view>
#include <cstdint>

namespace Net {

/**
 * @brief Enumeration of all possible error codes produced by the Net library.
 *
 * Used as the error type inside @c Result<T> (i.e. @c std::unexpected<Error>).
 * Errors are grouped by the operation that produces them. The underlying type
 * is @c uint8_t to keep the size minimal when stored in @c std::expected.
 *
 * @note @c Error::Ok (value @c 0) is the success state and is never placed
 *       inside @c std::unexpected — it exists for completeness and for use
 *       with @c toErrorString().
 */
enum class Error : uint8_t  {

    // -------------------------------------------------------------------------
    // General
    // -------------------------------------------------------------------------

    Ok                   = 0,  ///< No error — operation succeeded.
    UnknownError         = 1,  ///< An unrecognized platform error code was returned.

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    WSAStartupFailed     = 2,  ///< Windows only — @c WSAStartup() failed to initialize Winsock.
    SocketCreationFailed = 3,  ///< @c socket() returned an invalid handle.

    // -------------------------------------------------------------------------
    // Address / IP
    // -------------------------------------------------------------------------

    InvalidIP            = 4,  ///< The IP string could not be parsed by @c inet_pton().
    InvalidPort          = 5,  ///< Port was @c 0 or out of the valid range.
    InvalidAddressFamily = 6,  ///< @c ss_family / @c sin_family held an unrecognized value.
    InvalidSocketType    = 7,  ///< Unknown or unsupported @c SOCK_* type.
    InvalidProtocol      = 30, ///< Protocol is incompatible with the requested operation.

    // -------------------------------------------------------------------------
    // Bind
    // -------------------------------------------------------------------------

    BindFailed           = 8,  ///< @c bind() failed for a reason not covered below.
    AddressAlreadyInUse  = 9,  ///< The port is already occupied (@c EADDRINUSE / @c WSAEADDRINUSE).
    AddressNotAvailable  = 10, ///< The requested IP is not assigned to any local interface.

    // -------------------------------------------------------------------------
    // Listen
    // -------------------------------------------------------------------------

    ListenFailed         = 11, ///< @c listen() failed.
    NotBound             = 12, ///< @c listen() was called before @c bind().

    // -------------------------------------------------------------------------
    // Accept
    // -------------------------------------------------------------------------

    AcceptFailed         = 13, ///< @c accept() failed.
    NotListening         = 14, ///< @c accept() was called before @c listen().

    // -------------------------------------------------------------------------
    // Connect
    // -------------------------------------------------------------------------

    ConnectFailed        = 15, ///< @c connect() failed for a reason not covered below.
    ConnectionRefused    = 16, ///< The remote host actively refused the connection.
    ConnectionTimeout    = 17, ///< The connection attempt timed out before completing.
    AlreadyConnected     = 18, ///< @c connect() was called on an already-connected socket.

    // -------------------------------------------------------------------------
    // Send
    // -------------------------------------------------------------------------

    SendFailed           = 19, ///< @c send() failed for a reason not covered below.
    ConnectionReset      = 20, ///< The remote peer forcibly closed the connection mid-transfer.
    BrokenPipe           = 21, ///< POSIX only — attempt to write to a socket whose read end is closed.

    // -------------------------------------------------------------------------
    // Receive
    // -------------------------------------------------------------------------

    ReceiveFailed        = 22, ///< @c recv() failed for a reason not covered below.
    ConnectionClosed     = 23, ///< The remote peer closed the connection gracefully (@c recv() returned @c 0).

    // -------------------------------------------------------------------------
    // Close
    // -------------------------------------------------------------------------

    CloseFailed          = 24, ///< @c close() / @c closesocket() failed.

    // -------------------------------------------------------------------------
    // Socket State
    // -------------------------------------------------------------------------

    SocketNotInitialized = 25, ///< An operation was attempted before the socket was initialized.
    SocketAlreadyClosed  = 26, ///< An operation was attempted after the socket was closed.
    WouldBlock           = 27, ///< Non-blocking socket has no data ready yet (@c EAGAIN / @c WSAEWOULDBLOCK).

    SocketOptionFailed   = 28, ///< Setting a socket option failed.
    // Raw Socket
    // -------------------------------------------------------------------------

    RawSocketNotPermitted = 29, ///< Creating a raw socket requires root or Administrator privileges.
};

/**
 * @brief Retrieves the current platform error and maps it to a @c Net::Error.
 *
 * Calls @c getLastError() (which wraps @c errno on POSIX and
 * @c WSAGetLastError() on Windows) and maps the resulting integer code to the
 * closest @c Net::Error value. Also prints the raw integer code to stdout as a
 * debug trace.
 *
 * Platform mapping is compile-time selected:
 * - On Windows, Winsock @c WSAE* codes are mapped.
 * - On POSIX, standard @c E* codes are mapped.
 *
 * Any code that has no explicit mapping returns @c Error::UnknownError.
 *
 * @return The @c Net::Error value corresponding to the most recent platform
 *         socket error. Never returns @c Error::Ok.
 *
 * @throws Nothing — marked @c noexcept.
 */
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
        case WSAEMSGSIZE:        return Error::SendFailed;
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
        case WSAEHOSTUNREACH:    return Error::ConnectionClosed;
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
        case EMSGSIZE:           return Error::SendFailed;
        case ENOBUFS:            return Error::SendFailed;
        case EWOULDBLOCK:
            return Error::WouldBlock;
        case EINPROGRESS:        return Error::WouldBlock;
        case EALREADY:           return Error::WouldBlock;
        case ENOTSOCK:           return Error::SocketNotInitialized;
        case EINVAL:             return Error::InvalidSocketType;
        case EOPNOTSUPP:         return Error::InvalidSocketType;
        case EACCES:
        case EPERM:              return Error::RawSocketNotPermitted;
        case ENETDOWN:
        case ENETUNREACH:
        case EHOSTUNREACH:       return Error::ConnectFailed;
        default:                 return Error::UnknownError;
    }
#endif
}

/**
 * @brief Returns a human-readable description of a @c Net::Error value.
 *
 * Covers every enumerator in @c Error. If an unrecognized value is somehow
 * passed (e.g. via a cast), the fallback return is @c "Unrecognized error".
 *
 * The returned @c std::string_view points into static storage — it is always
 * valid and never null.
 *
 * Typical usage:
 * @code
 * auto result = address.getIp();
 * if (!result)
 *     std::println("Error: {}", Net::toErrorString(result.error()));
 * @endcode
 *
 * @param error  Any @c Net::Error enumerator, including @c Error::Ok.
 *
 * @return A non-owning @c std::string_view into a string literal describing
 *         @p error. The view is valid for the lifetime of the program.
 *
 * @throws Nothing — marked @c noexcept.
 */
inline std::string_view toErrorString(Error error) noexcept {
    switch (error) {
        case Error::Ok:                    return "Ok";
        case Error::UnknownError:          return "Unknown error";
        case Error::WSAStartupFailed:      return "Windows Winsock initialization failed";
        case Error::SocketCreationFailed:  return "Failed to create socket";
        case Error::SocketOptionFailed:    return "Setting a socket option failed";
        case Error::InvalidIP:             return "Invalid IP address format";
        case Error::InvalidPort:           return "Port out of range (0 or >65535)";
        case Error::InvalidAddressFamily:  return "Unknown IP type or incompatible address family";
        case Error::InvalidSocketType:     return "Unknown protocol type";
        case Error::InvalidProtocol:       return "Invalid protocol for this operation";
        case Error::BindFailed:            return "Failed to bind socket";
        case Error::AddressAlreadyInUse:   return "Address already in use (port occupied)";
        case Error::AddressNotAvailable:   return "Address not available on this machine";
        case Error::ListenFailed:          return "Failed to listen on socket";
        case Error::NotBound:              return "Cannot listen before binding";
        case Error::AcceptFailed:          return "Failed to accept connection";
        case Error::NotListening:          return "Cannot accept before listening";
        case Error::ConnectFailed:         return "Failed to connect";
        case Error::ConnectionRefused:     return "Connection refused by remote";
        case Error::ConnectionTimeout:     return "Connection timed out";
        case Error::AlreadyConnected:      return "Socket is already connected";
        case Error::SendFailed:            return "Failed to send data";
        case Error::ConnectionReset:       return "Connection forcibly closed by remote";
        case Error::BrokenPipe:            return "Cannot write to a closed socket";
        case Error::ReceiveFailed:         return "Failed to receive data";
        case Error::ConnectionClosed:      return "Connection closed gracefully by remote";
        case Error::CloseFailed:           return "Failed to close socket";
        case Error::SocketNotInitialized:  return "Socket not initialized, call init() first";
        case Error::SocketAlreadyClosed:   return "Socket is already closed";
        case Error::WouldBlock:            return "Non-blocking socket has no data yet";
        case Error::RawSocketNotPermitted: return "Raw socket requires root/admin privileges";
    }
    return "Unrecognized error";
}

} // namespace Net
