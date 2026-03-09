#pragma once
#include "types.hpp"
#include <functional>
constexpr size_t MAX_EVENTS = 10;

namespace Net {
#ifndef _WIN32
    class Poll {
    public:

        /** @param timeout epoll_wait timeout in ms. 0 = non-blocking, -1 = block forever, >0 = wait N ms then fire onTimeout. */
        static Result<Poll> create(int timeout = 3000) noexcept;

        /** @param ctx Optional user context. Poll does NOT own or free this pointer, you must reclaim it in onClose. */
        Result<void> add(int fd, Net::EpollEvent events, void* ctx = nullptr) noexcept;

        /** Updates events or context for an already-registered fd. */
        Result<void> mod(int fd, Net::EpollEvent events, void* ctx = nullptr) noexcept;

        /** Removes fd from epoll then fires onClose. Always goes through here to ensure ctx is reclaimed. */
        void close(int fd, void* ctx) noexcept;

        /** Blocks forever dispatching onRead, onWrite, onError, and onTimeout. */
        Result<void> watch() noexcept;

        /** Fired when fd has data ready to read (EPOLLIN). */
        void onRead(CallBackEvent cb);

        /** Fired when fd is ready to write (EPOLLOUT). */
        void onWrite(CallBackEvent cb);


        /** Fired on EPOLLERR or EPOLLHUP. Call close(fd, ctx) from here. */
        void onError(ErrorCallBack cb);

        /**
         * Fired after close() removes the fd. You MUST reclaim ctx here or it leaks —
         * epoll does not free it, ever.
         *
         *   poller.onClose([](void* ctx) {
         *       std::unique_ptr<MyCtx>{ static_cast<MyCtx*>(ctx) };
         *   });
         */
        void onClose(CallBackEvent cb);

        /** Fired when epoll_wait times out. Use for idle work: pinging clients, expiring stale connections, etc. */
        void onTimeout(std::function<void()> cb);

        /** Returns the underlying epoll fd. */
        int fd() const noexcept;
        ~Poll();

        Poll(const Poll&)            = delete;
        Poll& operator=(const Poll&) = delete;

        Poll(Poll&& other) noexcept;
        Poll& operator=(Poll&& other) noexcept;

    private:
        struct epoll_event ev_{}, events_[MAX_EVENTS];
        int efd_{-1};
        int timeout_{-1};

        CallBackEvent onRead_ {nullptr};
        CallBackEvent onWrite_ {nullptr};
        ErrorCallBack onError_ {nullptr};
        CallBackEvent onClose_ {nullptr};
        std::function<void()> onTimeout_{nullptr};


        Poll(int fd, int timeout);
    };
#endif
} // namespace Net
