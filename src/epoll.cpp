#include "../include/epoll.hpp"
#include <algorithm>
#include <expected>

namespace Net::Poll {


Watcher::Watcher(int fd, int timeout) noexcept
    : ev_{}, events_{}, efd_{fd}, timeout_{timeout} {}

Result<Watcher> Watcher::create(int timeout) noexcept {
    int efd = epoll_create1(0);
    if (efd == -1)
        return std::unexpected{getError()};
    return Watcher{efd, timeout};
}


Result<void> Watcher::addImpl(Descriptor* desc, Net::EpollEvent events) noexcept {
    struct epoll_event ev{};
    ev.events   = static_cast<uint32_t>(events);
    ev.data.ptr = desc;  // store the Descriptor* directly — fd lives inside it
    if (epoll_ctl(efd_, static_cast<int>(Net::EpollOptions::Add), desc->fd, &ev) == -1)
        return std::unexpected{getError()};
    return {};
}

Result<void> Watcher::modImpl(Descriptor* desc, Net::EpollEvent events) noexcept {
    struct epoll_event ev{};
    ev.events   = static_cast<uint32_t>(events);
    ev.data.ptr = desc;
    if (epoll_ctl(efd_, static_cast<int>(Net::EpollOptions::Mod), desc->fd, &ev) == -1)
        return std::unexpected{getError()};
    return {};
}

void Watcher::closeImpl(Descriptor* desc) noexcept {
    struct epoll_event ev{};
    epoll_ctl(efd_, static_cast<int>(Net::EpollOptions::Del), desc->fd, &ev);
    if (onClose_) onClose_(desc);
}


Result<void> Watcher::watch() noexcept {
    for (;;) {
        int nfds = epoll_wait(efd_, events_, MAX_EVENTS, timeout_);

        if (nfds == 0) {
            if (onTimeout_) onTimeout_();
            continue;
        }

        if (nfds == -1) {
            if (errno == EINTR) continue;
            return std::unexpected{getError()};
        }

        for (int i = 0; i < nfds; i++) {
            auto* desc = static_cast<Descriptor*>(events_[i].data.ptr);
            uint32_t ev = events_[i].events;

            if (ev & (static_cast<uint32_t>(Net::EpollEvent::EPOLLERR) |static_cast<uint32_t>(Net::EpollEvent::EPOLLHUP))) {
                if (onError_) {
                    int err = 0;
                    socklen_t len = sizeof(err);
                    getsockopt(desc->fd, SOL_SOCKET, SO_ERROR, &err, &len);
                    errno = err ? err : EIO;
                    onError_(desc, getError());
                }
                continue;
            }

            if (ev & static_cast<uint32_t>(Net::EpollEvent::EPOLLRDHUP)) {
                closeImpl(desc);
                continue;
            }

            if (ev & static_cast<uint32_t>(Net::EpollEvent::EPOLLIN))
                if (onRead_) {
                    onRead_(desc);
                    continue;
                }

            if (ev & static_cast<uint32_t>(Net::EpollEvent::EPOLLOUT))
                if (onWrite_) onWrite_(desc);

        }
    }
}



int Watcher::fd() const noexcept { return efd_; }


Watcher::~Watcher() {
    if (efd_ != -1) ::close(efd_);
}

Watcher::Watcher(Watcher&& other) noexcept
    : ev_{other.ev_}, efd_{other.efd_}, timeout_{other.timeout_}
{
    std::copy_n(other.events_, MAX_EVENTS, events_);
    other.efd_ = -1;
}

Watcher& Watcher::operator=(Watcher&& other) noexcept {
    if (this != &other) {
        if (efd_ != -1) ::close(efd_);
        ev_      = other.ev_;
        efd_     = other.efd_;
        timeout_ = other.timeout_;
        std::copy_n(other.events_, MAX_EVENTS, events_);
        other.efd_ = -1;
    }
    return *this;
}

} // namespace Net::Poll
