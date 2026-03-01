#pragma once

/**
 * @file platform.hpp
 * @brief Platform abstraction layer for socket programming.
 *
 * Provides a unified interface over the Windows Winsock2 and POSIX socket
 * APIs. Including this header is sufficient to use sockets portably — no
 * platform guards are needed in consuming code.
 *
 * All platform-specific types, constants, and inline functions are normalized
 * to a common interface:
 *
 * | Abstraction       | Windows             | POSIX          |
 * |-------------------|---------------------|----------------|
 * | @c SocketHandle   | @c SOCKET           | @c int         |
 * | @c invaliedSocket | @c INVALID_SOCKET   | @c -1          |
 * | @c ssize          | @c int              | @c ssize_t     |
 * | @c getLastError() | @c WSAGetLastError()| @c errno       |
 * | @c platformClose()| @c closesocket()    | @c close()     |
 */

#ifdef _WIN32

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN  ///< Exclude rarely-used Windows headers to reduce compile time.
    #endif

    #include <winsock2.h>
    #include <ws2tcpip.h>   // inet_pton, inet_ntop, getaddrinfo
    #include <ws2ipdef.h>
    #pragma comment(lib, "ws2_32.lib")

    /**
     * @brief Platform socket handle type.
     *
     * Aliases @c SOCKET on Windows and @c int on POSIX. Always check against
     * @c invaliedSocket rather than a hard-coded value for portability.
     */
    using SocketHandle = SOCKET;

    /**
     * @brief Sentinel value representing an invalid or uninitialized socket.
     *
     * Equals @c INVALID_SOCKET on Windows and @c -1 on POSIX.
     * Use this constant instead of a hard-coded value in comparisons.
     */
    constexpr SocketHandle invaliedSocket = INVALID_SOCKET;

    /**
     * @brief Returns the most recent socket error code on Windows.
     *
     * Wraps @c WSAGetLastError(), which retrieves the error set by the last
     * failed Winsock API call on the calling thread. Must be called
     * immediately after a failing call before any other Winsock function
     * clears the error state.
     *
     * @return The last Winsock error code (e.g. @c WSAECONNREFUSED).
     *
     * @throws Nothing — marked @c noexcept.
     */
    inline int getLastError() noexcept {
        return WSAGetLastError();
    }

    /**
     * @brief Closes a socket handle on Windows.
     *
     * Wraps @c ::closesocket(). Safe to call only once per handle — calling
     * it on an already-closed handle results in a @c WSAENOTSOCK error from
     * Winsock (which this function discards).
     *
     * @param s  A valid @c SocketHandle to close.
     *
     * @throws Nothing — marked @c noexcept.
     */
    inline void platformClose(SocketHandle s) noexcept { ::closesocket(s); }

    /**
     * @brief Signed integer type for byte counts returned by @c send() / @c recv().
     *
     * Aliases @c int on Windows (Winsock returns @c int) and @c ssize_t on POSIX.
     */
    using ssize = int;

#else

    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    #include <fcntl.h>

    /**
     * @brief Platform socket handle type.
     *
     * Aliases @c int on POSIX and @c SOCKET on Windows. Always check against
     * @c invaliedSocket rather than a hard-coded value for portability.
     */
    using SocketHandle = int;

    /**
     * @brief Sentinel value representing an invalid or uninitialized socket.
     *
     * Equals @c -1 on POSIX and @c INVALID_SOCKET on Windows.
     * Use this constant instead of a hard-coded value in comparisons.
     */
    constexpr SocketHandle invaliedSocket = -1;

    /**
     * @brief Returns the most recent socket error code on POSIX.
     *
     * Reads @c errno directly. Must be called immediately after a failing
     * system call — any subsequent call that succeeds will reset @c errno
     * to @c 0, losing the original error.
     *
     * @return The current value of @c errno (e.g. @c ECONNREFUSED).
     *
     * @throws Nothing — marked @c noexcept.
     */
    inline int getLastError() noexcept {
        return errno;
    }

    /// @copydoc ssize
    using ssize = ssize_t;

    /**
     * @brief Closes a socket handle on POSIX.
     *
     * Wraps @c ::close(). Safe to call only once per file descriptor —
     * calling it on an already-closed descriptor has undefined behavior
     * under POSIX (the fd may have been reused by another thread).
     *
     * @param s  A valid @c SocketHandle to close.
     *
     * @throws Nothing — marked @c noexcept.
     */
    inline void platformClose(SocketHandle s) noexcept { ::close(s); }

#endif
