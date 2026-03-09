
#include "../include/epoll.hpp"
#include <algorithm>
#include <expected>

namespace Net {

    Poll::Poll(int fd, int timeout)
        : ev_{}, events_{}, efd_{fd}, timeout_{timeout} {}

    Result<Poll> Poll::create(int timeout) noexcept {
        int efd = epoll_create1(0);
        if (efd == -1)
            return std::unexpected{getError()};
        return Poll{efd, timeout};
    }

    Result<void> Poll::add(int fd, Net::EpollEvent events, void* ctx) noexcept {
        struct epoll_event ev{};
        ev.events   = static_cast<uint32_t>(events);
        ev.data.ptr = ctx;
        if (epoll_ctl(efd_, static_cast<int>(Net::EpollOptions::Add), fd, &ev) == -1)
            return std::unexpected{getError()};
        return {};
    }

    Result<void> Poll::mod(int fd, Net::EpollEvent events, void* ctx) noexcept {
        struct epoll_event ev{};
        ev.events   = static_cast<uint32_t>(events);
        ev.data.ptr = ctx;
        if (epoll_ctl(efd_, static_cast<int>(Net::EpollOptions::Mod), fd, &ev) == -1)
            return std::unexpected{getError()};
        return {};
    }

    void Poll::close(int fd, void* ctx) noexcept {
        struct epoll_event ev{};
        epoll_ctl(efd_, static_cast<int>(Net::EpollOptions::Del), fd, &ev);
        if (onClose_) onClose_(ctx);
    }

    Result<void> Poll::watch() noexcept {
        for (;;) {
            int nfds = epoll_wait(fd(), events_, MAX_EVENTS, timeout_);
            if (nfds == 0)  {if (onTimeout_) onTimeout_(); continue;};
            if (nfds == -1) {
                       if (errno == EINTR) continue;
                       return std::unexpected{getError()};
                   }

            for (int i = 0; i < nfds; i++) {
                void*  ctx = events_[i].data.ptr;
                uint32_t ev  = events_[i].events;


                if (ev & static_cast<uint32_t>(Net::EpollEvent::EPOLLRDHUP))
                    if (onClose_) onClose_(ctx);

                if (ev & static_cast<uint32_t>(Net::EpollEvent::EPOLLIN))
                    if (onRead_) onRead_(ctx);

                if (ev & static_cast<uint32_t>(Net::EpollEvent::EPOLLOUT))
                    if (onWrite_) onWrite_(ctx);

                if (ev & (static_cast<uint32_t>(Net::EpollEvent::EPOLLERR) |
                           static_cast<uint32_t>(Net::EpollEvent::EPOLLHUP)))
                    if (onError_) {
                        int  err = 0;
                        socklen_t len = sizeof(err);
                        // TODO: get the event fd;
                        getsockopt(fd(), SOL_SOCKET, SO_ERROR, &err, &len);
                        errno = err ? err : EIO;
                        onError_(ctx,getError());
                    }

            }
        }
    }

    void Poll::onRead(CallBackEvent cb)  { onRead_    = std::move(cb); }
    void Poll::onWrite (CallBackEvent cb) { onWrite_   = std::move(cb); }
    void Poll::onError(ErrorCallBack cb)   { onError_   = std::move(cb); }
    void Poll::onClose (CallBackEvent cb){ onClose_   = std::move(cb); }
    void Poll::onTimeout (std::function<void()> cb) { onTimeout_ = std::move(cb); }

    int Poll::fd() const noexcept { return efd_; }

    Poll::~Poll() { if (efd_ != -1) ::close(efd_); }

    Poll::Poll(Poll&& other) noexcept
        : ev_{other.ev_}, efd_{other.efd_}, timeout_{other.timeout_}
    {
        std::copy_n(other.events_, MAX_EVENTS, events_);
        other.efd_ = -1;
    }

    Poll& Poll::operator=(Poll&& other) noexcept {
        if (this != &other) {
            if (efd_ != -1) ::close(efd_);
            ev_   = other.ev_;
            efd_ = other.efd_;
            timeout_ = other.timeout_;
            std::copy_n(other.events_, MAX_EVENTS, events_);
            other.efd_ = -1;
        }
        return *this;
    }

} // namespace Net
