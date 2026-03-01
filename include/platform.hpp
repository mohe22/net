#pragma once


#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>   // inet_pton, inet_ntop, getaddrinfo
    #include <ws2ipdef.h>
    #pragma comment(lib, "ws2_32.lib")

    using SocketHandle = SOCKET;
    constexpr SocketHandle invaliedSocket = INVALID_SOCKET;
    inline int getLastError() noexcept {
        return WSAGetLastError();
    }
    inline void platformClose(SocketHandle s) noexcept { ::closesocket(s); }
    using ssize = int;
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    #include <fcntl.h>

    using SocketHandle = int;
    constexpr SocketHandle invaliedSocket = -1;
    inline int getLastError() noexcept {
            return errno;
    }
    using ssize = ssize_t;
    inline void platformClose(SocketHandle s) noexcept { ::close(s); }
#endif
