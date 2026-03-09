#pragma once
#include "types.hpp"
#include <functional>

constexpr size_t MAX_EVENTS = 10;

namespace Net::Poll {
#ifndef _WIN32

/**
 * @brief Base struct every user defined context MUST publicly inherit from it.
 *
 * The Watcher stores your pointer directly in epoll's data.ptr,the Watcher can
 * always recover the fd from the pointer it gets back from epoll_wait.
 *
 * @code
 *   struct MyConn : public Net::Poll::Descriptor {
 *       std::string name;
 *       int counter = 0;
 *   };
 *
 *   MyConn conn;
 *   conn.fd = sock_fd;
 *   watcher.add(&conn, Net::EpollEvent::EPOLLIN);
 * @endcode
 */
struct Descriptor {
    SocketHandle fd;
};

/**
 * @brief Compile time constraint: T must publicly inherit Descriptor.
 *
 * Enforced on every add / mod / close call. Passing an unrelated type
 * is a hard compile error — nothing unsafe reaches runtime.
 */
template<typename T>
concept Watchable = std::derived_from<T, Descriptor>;

class Watcher {
public:

    /**
     * @brief Creates a new epoll instance.
     * @param timeout epoll_wait timeout in ms.
     *                0  = non-blocking (returns immediately if no events).
     *               -1  = block forever.
     *               >0  = wait N ms, then fire onTimeout.
     * @return Watcher on success, error on failure.
     */
    static Result<Watcher> create(int timeout = 3000) noexcept;

    /**
     * @brief Registers a descriptor with epoll.
     *
     * Watcher stores desc as a Descriptor* in epoll's data.ptr.
     * It never owns or frees the pointer — lifetime is your responsibility.
     *
     * @tparam T     Must publicly inherit Net::Poll::Descriptor.
     * @param  desc  Caller-owned context pointer.
     * @param  events Bitmask of EpollEvent flags to watch for.
     * @return Error if epoll_ctl fails.
     */
    template<Watchable T>
    Result<void> add(T* desc, Net::EpollEvent events) noexcept {
        return addImpl(static_cast<Descriptor*>(desc), events);
    }

    /**
     * @brief Updates the watched events for an already-registered descriptor.
     *
     * @tparam T     Must publicly inherit Net::Poll::Descriptor.
     * @param  desc  Same pointer that was passed to add().
     * @param  events New event bitmask.
     * @return Error if epoll_ctl fails.
     */
    template<Watchable T>
    Result<void> mod(T* desc, Net::EpollEvent events) noexcept {
        return modImpl(static_cast<Descriptor*>(desc), events);
    }

    /**
     * @brief Removes a descriptor from epoll and fires onClose.
     *
     * Always remove fds through here — skipping this leaks your context
     * because epoll will never fire onClose on its own.
     *
     * @tparam T    Must publicly inherit Net::Poll::Descriptor.
     * @param  desc Pointer to the context to remove.
     */
    template<Watchable T>
    void close(T* desc) noexcept {
        closeImpl(static_cast<Descriptor*>(desc));
    }

    /**
     * @brief Blocks and dispatches events until an unrecoverable error occurs.
     *
     * Fires onRead, onWrite, onError, onClose, and onTimeout as events arrive.
     * EINTR is silently retried. Only returns on a fatal epoll_wait error.
     *
     * @return Error describing why the loop exited.
     */
    Result<void> watch() noexcept;

    /**
     * @brief Registers a callback fired when a descriptor has data to read (EPOLLIN).
     *
     *   recovers T* from epoll's void* before calling you —
     * you never deal with raw pointers or casts.
     *
     * @tparam T   Your concrete type — must inherit Descriptor.
     * @param  cb  Called with a pointer directly to your context.
     *
     * @code
     *   poller.onRead<MyConn>([](MyConn* conn) {
     *       conn->receive(...);
     *   });
     * @endcode
     */
    template<Watchable T>
    void onRead(std::function<void(T*)> cb) {
        // Wrap the user's typed callback in a void* lambda for internal storage.
        // The cast chain void* -> Descriptor* -> T* is always safe because
        // addImpl only ever stores Descriptor* (or a subclass) in data.ptr.
        onRead_ = [cb](void* raw) {
            cb(static_cast<T*>(static_cast<Descriptor*>(raw)));
        };
    }

    /**
     * @brief Registers a callback fired when a descriptor is ready to write (EPOLLOUT).
     *
     * @tparam T   Your concrete type — must inherit Descriptor.
     * @param  cb  Called with a pointer directly to your context.
     *
     * @code
     *   poller.onWrite<MyConn>([](MyConn* conn) {
     *       conn->send(...);
     *   });
     * @endcode
     */
    template<Watchable T>
    void onWrite(std::function<void(T*)> cb) {
        onWrite_ = [cb](void* raw) {
            cb(
                static_cast<T*>(
                    static_cast<Descriptor*>(raw)
                )
            );
        };
    }

    /**
     * @brief Registers a callback fired after close() removes a descriptor from epoll.
     *
     * This is your ONLY opportunity to reclaim the context — epoll never frees it.
     * The fd has already been removed from epoll before this fires, so it is
     * safe to free or destroy the context here.
     *
     * @tparam T   Your concrete type — must inherit Descriptor.
     * @param  cb  Called with a pointer directly to your context.
     *
     * @code
     *   // Stack-allocated — nothing to free, just log
     *   poller.onClose<MyConn>([](MyConn* conn) {
     *       std::println("closed fd={}", conn->fd);
     *   });
     *
     *   // Heap-allocated — you must delete here or you leak
     *   poller.onClose<MyConn>([](MyConn* conn) {
     *       delete conn;
     *   });
     *
     *   // Owned by a map via unique_ptr — erase triggers the destructor
     *   poller.onClose<MyConn>([&](MyConn* conn) {
     *       clients.erase(conn->fd);
     *   });
     * @endcode
     */
    template<Watchable T>
    void onClose(std::function<void(T*)> cb) {
        onClose_ = [cb](void* raw) {
            cb(
                static_cast<T*>(
                    static_cast<Descriptor*>(raw)
                )
            );
        };
    }

    /**
     * @brief Registers a callback fired on EPOLLERR or EPOLLHUP.
     *
     * The underlying socket error is resolved via getsockopt(SO_ERROR)
     * and forwarded as a Net::Error. You should call close() from here
     * to deregister the fd and trigger onClose for cleanup.
     *
     * @tparam T   Your concrete type — must inherit Descriptor.
     * @param  cb  Called with a pointer to your context and the resolved error.
     *
     * @code
     *   poller.onError<MyConn>([&](MyConn* conn, Net::Error err) {
     *       std::println("error fd={}: {}", conn->fd, Net::toErrorString(err));
     *       poller.close(conn);  // triggers onClose where you free conn
     *   });
     * @endcode
     */
    template<Watchable T>
    void onError(std::function<void(T*, Net::Error)> cb) {
        onError_ = [cb](void* raw, Net::Error err) {
            cb(
                static_cast<T*>(
                    static_cast<Descriptor*>(raw)
                ),
                err
            );
        };
    }

    /**
     * @brief Registers a callback fired when epoll_wait times out.
     *
     * Only fires when timeout > 0 was passed to create().
     * Use for idle work: heartbeats, expiring stale connections, etc.
     *
     * @param cb Called with no arguments on each timeout.
     */
    void onTimeout(std::function<void()> cb){
        onTimeout_ = std::move(cb);
    }
    /** @brief Returns the underlying epoll file descriptor. */
    int fd() const noexcept;

    ~Watcher();
    Watcher(const Watcher&)            = delete;
    Watcher& operator=(const Watcher&) = delete;
    Watcher(Watcher&&)            noexcept;
    Watcher& operator=(Watcher&&) noexcept;

private:
    /// Non-template impls — all real epoll logic lives in the .cpp,
    /// keeping system headers and implementation details out of the public header.
    Result<void> addImpl  (Descriptor* desc, Net::EpollEvent events) noexcept;
    Result<void> modImpl  (Descriptor* desc, Net::EpollEvent events) noexcept;
    void         closeImpl(Descriptor* desc)                         noexcept;

    struct epoll_event ev_{}, events_[MAX_EVENTS];
    int efd_    {-1};
    int timeout_{-1};

    /// Internal storage is always void*-based — T is erased at this boundary.
    /// The templated setters above wrap the user's typed callback in a lambda
    /// that performs the cast, then assign it here. The cast logic never
    /// escapes into user code.
    std::function<void(void*)>             onRead_   {nullptr};
    std::function<void(void*)>             onWrite_  {nullptr};
    std::function<void(void*, Net::Error)> onError_  {nullptr};
    std::function<void(void*)>             onClose_  {nullptr};
    std::function<void()>                  onTimeout_{nullptr};

    explicit Watcher(int fd, int timeout) noexcept;
};

#endif
} // namespace Net::Poll
