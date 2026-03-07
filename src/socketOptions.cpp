#include "../include/socketOptions.hpp"

namespace Net {
    Result<void> SocketOptions::setNonBlocking(bool enabled)  noexcept {
    #ifdef _WIN32
        u_long mode = enabled ? 1 : 0;
        if (ioctlsocket(fd_, FIONBIO, &mode) == SOCKET_ERROR)
            return std::unexpected{Net::Error::SocketOptionFailed};
    #else
        int flags = fcntl(getSocket(), F_GETFL, 0);
        if (flags == -1)
            return std::unexpected{Net::Error::InvalidSocketType};
        flags = enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
        if (fcntl(getSocket(), F_SETFL, flags) == -1)
            return std::unexpected{Net::Error::SocketOptionFailed};
    #endif

        isBlocking_ = !enabled;
        return {};
    }
    Result<void> SocketOptions::setTimeoutOption(SocketOption option,
        std::chrono::milliseconds timeout) const noexcept {
    #ifdef _WIN32
        DWORD ms = static_cast<DWORD>(timeout.count());
        return setOption(option, &ms, sizeof(ms));
    #else
        struct timeval tv;
        tv.tv_sec  = std::chrono::duration_cast<std::chrono::seconds>(timeout).count();
        tv.tv_usec = (timeout.count() % 1000) * 1000;
        return setOption(option, &tv, sizeof(tv));
    #endif
    }

    Result<void> SocketOptions::setOption(SocketOption option,
        const void* optval, int optvalSize) const noexcept {
            int result = ::setsockopt(
                getSocket(),
                getOptionLevel(option),
                static_cast<int>(option),
                optval,
                optvalSize);
        if (result == -1) return std::unexpected{Net::getError()};
        return {};
    }

} // namespace Net
