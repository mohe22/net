#pragma once


/**
 * @brief Platform abstraction layer for socket programming.
 */
#ifdef _WIN32
    /*
    * according to :https://stackoverflow.com/questions/11040133/what-does-defining-win32-lean-and-mean-exclude-exactly
    *  using: WIN32_LEAN_AND_MEAN it will excludes APIs such as Cryptography, DDE, RPC, Shell, and Windows Sockets.
    *  makeing much faster compile time.
    */
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    #include <winsock2.h>  // Core Windows Sockets API. Provide SOCKET, sockaddr, bind(), listen(),  etc.
    #include <ws2tcpip.h>   // Modern TCP/IP extensions for Winsock. Provide:inet_pton(), inet_ntop()
    #include <ws2ipdef.h> // Low-level IP protocol definitions. Provide: IN6_ADDR, sockaddr_in6 details
    #pragma comment(lib, "ws2_32.lib")

    namespace Net {

        /**
         * @brief Platform socket handle type.
         *
         * Aliases @c SOCKET on Windows and @c int on POSIX. Always check against
         * @c invalidSocket rather than a hard-coded value for portability.
         */
        using SocketHandle = SOCKET;

        /**
         * @brief Sentinel value representing an invalid or uninitialized socket.
         *
         * Equals @c INVALID_SOCKET on Windows and @c -1 on POSIX.
         * Use this constant instead of a hard-coded value in comparisons.
         */
        constexpr SocketHandle invalidSocket = INVALID_SOCKET;

        /**
         * @brief Signed integer type for byte counts returned by @c send() / @c recv().
         *
         * Aliases @c int on Windows (Winsock returns @c int) and @c ssize_t on POSIX.
         */
        using ssize = int;

        /**
         * @brief Returns the most recent socket error code on Windows.
         *
         * Wraps @c WSAGetLastError(), which retrieves the error set by the last
         * failed Winsock API call on the calling thread. Must be called
         * immediately after a failing call before any other Winsock function
         * clears the error state.
         *
         * @return The last Winsock error code (e.g. @c WSAECONNREFUSED).
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
         * @throws Nothing — marked @c noexcept.
         */
        inline void platformClose(SocketHandle s) noexcept {
            ::closesocket(s);
        }

    } // namespace Net

#else

    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    #include <fcntl.h>

    namespace Net {

        /**
         * @brief Platform socket handle type.
         *
         * Aliases @c int on POSIX and @c SOCKET on Windows. Always check against
         * @c invalidSocket rather than a hard-coded value for portability.
         */
        using SocketHandle = int;

        /**
         * @brief Sentinel value representing an invalid or uninitialized socket.
         *
         * Equals @c -1 on POSIX and @c INVALID_SOCKET on Windows.
         * Use this constant instead of a hard-coded value in comparisons.
         */
        constexpr SocketHandle invalidSocket = -1;

        /**
         * @brief Signed integer type for byte counts returned by @c send() / @c recv().
         *
         * Aliases @c ssize_t on POSIX and @c int on Windows.
         */
        using ssize = ssize_t;

        /**
         * @brief Returns the most recent socket error code on POSIX.
         *
         * Reads @c errno directly. Must be called immediately after a failing
         * system call — any subsequent call that succeeds will reset @c errno
         * to @c 0, losing the original error.
         *
         * @return The current value of @c errno (e.g. @c ECONNREFUSED).
         * @throws Nothing — marked @c noexcept.
         */
        inline int getLastError() noexcept {
            return errno;
        }

        /**
         * @brief Closes a socket handle on POSIX.
         *
         * Wraps @c ::close(). Safe to call only once per file descriptor —
         * calling it on an already-closed descriptor has undefined behavior
         * under POSIX (the fd may have been reused by another thread).
         *
         * @param s  A valid @c SocketHandle to close.
         * @throws Nothing — marked @c noexcept.
         */
        inline void platformClose(SocketHandle s) noexcept {
            ::close(s);
        }

    } // namespace Net

#endif
