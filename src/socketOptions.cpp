#include "../include/socketOptions.hpp"

namespace Net {

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
    #ifdef _WIN32
        const char* optptr = reinterpret_cast<const char*>(optval);
    #else
        const void* optptr = optval;
    #endif
        int result = ::setsockopt(
            getSocket(),
            getOptionLevel(option),
            static_cast<int>(option),
            optptr,
            optvalSize
        );
        if (result == -1) return std::unexpected{Net::getError()};
        return {};
    }

} // namespace Net
