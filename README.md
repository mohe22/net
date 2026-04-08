# Net — C++ Cross-Platform Socket Library

A lightweight, modern C++ networking library built on top of raw POSIX and Winsock2 sockets.
It provides a clean, type-safe API using `std::expected` for error handling instead of exceptions,
and supports both TCP and UDP over IPv4 and IPv6.

---

## Project Structure

```
net/
├── include/
│   ├── platform.hpp       # Platform abstraction (Winsock2 vs POSIX)
│   ├── error.hpp          # Error enum + getError() + toErrorString()
│   ├── types.hpp          # Result<T>, IPType, Protocol, helper converters
│   ├── address.hpp        # Address class (IPv4/IPv6 wrapper over sockaddr_storage)
│   ├── connection.hpp     # Connection class (owns a connected TCP socket)
│   ├── socketOptions.hpp  # SocketOptions mixin (timeouts, buffers, keep-alive, etc.)
│   ├── server.hpp         # SocketBase, Tcp, Udp server classes
│   ├── epoll.hpp          # Poll class (epoll-based event loop, Linux only)
│   └── net.hpp            # Top-level convenience include
└── src/
    ├── address.cpp        # Address factory implementations
    ├── connection.cpp     # Connection send/receive/close
    ├── socketOptions.cpp  # setOption / setTimeoutOption implementations
    ├── server.cpp         # SocketBase init/bind, Tcp listen/accept, Udp sendTo/receiveFrom
    ├── epoll.cpp          # Poll event loop implementation
    ├── test-udp.cpp       # UDP usage example
    └── test_tcp.cpp       # TCP usage example
```

---

## Architecture

```
┌─────────────────────────────────────┐
│         Tcp / Udp (server.hpp)      │  ← Protocol-specific server logic
├─────────────────────────────────────┤
│         SocketBase (server.hpp)     │  ← Shared socket lifecycle (init, bind, close)
├─────────────────────────────────────┤
│      SocketOptions (mixin)          │  ← Socket options (timeouts, buffers, flags)
├─────────────────────────────────────┤
│     Connection (connection.hpp)     │  ← Owns an accepted/connected socket
├─────────────────────────────────────┤
│         Address (address.hpp)       │  ← Wraps sockaddr_storage (IPv4 + IPv6)
├─────────────────────────────────────┤
│         types.hpp / error.hpp       │  ← Result<T>, Error, IPType, Protocol
├─────────────────────────────────────┤
│         platform.hpp                │  ← SocketHandle, ssize, getLastError(), platformClose()
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│         Poll (epoll.hpp)            │  ← Event loop (Linux only, sits alongside the above)
└─────────────────────────────────────┘
```

---

## Headers

### `platform.hpp`

The foundation of the library. Conditionally includes either `<winsock2.h>` (Windows) or the
standard POSIX socket headers and normalizes everything behind a common interface.

| Abstraction        | Windows             | POSIX         |
|--------------------|---------------------|---------------|
| `SocketHandle`     | `SOCKET`            | `int`         |
| `invalidSocket`    | `INVALID_SOCKET`    | `-1`          |
| `ssize`            | `int`               | `ssize_t`     |
| `getLastError()`   | `WSAGetLastError()` | `errno`       |
| `platformClose(s)` | `::closesocket(s)`  | `::close(s)`  |

No other file contains `#ifdef _WIN32` for these concerns — they all go through `platform.hpp`.

---

### `error.hpp`

Defines `Net::Error` (`uint8_t`) covering every failure the library can produce, grouped by
operation: initialization, address/IP, bind, listen, accept, connect, send, receive, close,
and socket state.

- **`getError()`** — reads `getLastError()` and maps the raw platform code to a `Net::Error`.
- **`toErrorString(Error)`** — maps any `Net::Error` to a human-readable `std::string_view`.

---

### `types.hpp`

Defines the types and free functions used across the entire library.

- **`Result<T>`** — alias for `std::expected<T, Net::Error>`. Every fallible function returns this.
- **`RecvFromResult`** — `std::tuple<ssize, Address>` returned by `Udp::receiveFrom()`.
- **`Protocol`** — `TCP`, `UDP`, `RAW`.
- **`IPType`** — `IPv4`, `IPv6`.
- **`toAddressFamily(IPType)`** — `IPType` → `AF_INET` / `AF_INET6`.
- **`toSocketType(Protocol)`** — `Protocol` → `SOCK_STREAM` / `SOCK_DGRAM` / `SOCK_RAW`.
- **`fromAddressFamily(int)`** — `AF_*` → `IPType`.
- **`getAddressSizeByIpType(IPType)`** — `sizeof(sockaddr_in)` or `sizeof(sockaddr_in6)`.

---

### `address.hpp` / `address.cpp`

`Address` wraps a `sockaddr_storage` large enough to hold either an IPv4 or IPv6 address.
Constructed only through static factory methods, all of which return `Result<Address>`:

- **`Address::from(string, port)`** — parses a human-readable IP string (IPv4 first, then IPv6)
  and a port. Stores the IP string directly into `ip_`. Fails with `Error::InvalidIP` or
  `Error::InvalidPort`.
- **`Address::from(sockaddr_storage)`** — used after `accept()` / `recvfrom()` to wrap the peer
  address the OS wrote back. Delegates to the `sockaddr_in` or `sockaddr_in6` overload based on
  `ss_family`.
- **`Address::from(sockaddr_in)`** / **`Address::from(sockaddr_in6)`** — construct directly from
  a pre-populated struct. Uses `inet_ntop()` to build the human-readable IP string stored in
  `ip_`. Returns `Error::InvalidIP` if `inet_ntop()` fails.

`getIp()` returns a `string_view` into the internal `ip_` member — no computation on each call,
valid for the lifetime of the `Address` object. `getPort()`, `getIpType()` also return
`Result<T>`.

---

### `socketOptions.hpp` / `socketOptions.cpp`

`SocketOptions` is a mixin. Inherit from it and implement `getSocket()` to get all socket
configuration methods without duplicating any code between `SocketBase` and `Connection`.

```cpp
class MySocket : public Net::SocketOptions {
public:
    SocketHandle getSocket() const noexcept override { return socket_; }
};
```

The mixin has no data members. It calls `getSocket()` at the moment each option is applied so
the handle is always current.

| Method | Option | Description |
|---|---|---|
| `setReceiveTimeout(ms)` | `SO_RCVTIMEO` | Unblocks `recv()` after the given duration. `0ms` = block forever. |
| `setSendTimeout(ms)` | `SO_SNDTIMEO` | Unblocks `send()` if the send buffer stays full too long. `0ms` = block forever. |
| `setReceiveBuffer(bytes)` | `SO_RCVBUF` | Kernel receive buffer size. |
| `setSendBuffer(bytes)` | `SO_SNDBUF` | Kernel send buffer size. |
| `setReuseAddress(bool)` | `SO_REUSEADDR` | Lets the server restart without waiting for `TIME_WAIT`. |
| `setReusePort(bool)` | `SO_REUSEPORT` | Lets multiple sockets bind the same port. |
| `setKeepAlive(bool)` | `SO_KEEPALIVE` | OS sends probes on idle connections to detect dead peers. |
| `setNoDelay(bool)` | `TCP_NODELAY` | Disables Nagle's algorithm for lower latency. |
| `setBroadcast(bool)` | `SO_BROADCAST` | Required before sending to a broadcast address on UDP. |

> **Note on send timeout:** `SO_SNDTIMEO` only fires when the kernel send buffer is completely full.
> It will not fire for small writes since those fit in the buffer immediately and `send()` returns
> right away. To detect a connection that disconnected before you send, use `MSG_PEEK` instead.

Both `SocketBase` and `Connection` inherit `SocketOptions`, so all methods above are available on
server sockets and accepted connections alike.

---

### `connection.hpp` / `connection.cpp`

`Connection` represents one side of an established TCP connection. Owns a `SocketHandle` and the
remote `Address`. Created by `Tcp::accept()` and returned as `std::unique_ptr<Connection>`.

Non-copyable and non-movable — a socket is a unique OS resource.

Inherits `SocketOptions` so timeouts and all other options are available directly.

| Method | Description |
|---|---|
| `send(data, size)` | `::send()` — returns bytes sent or an error. |
| `receive(data, size)` | `::recv()` — returns bytes received or an error. |
| `close()` | Closes the socket explicitly. Destructor closes it too if not already done. |

---

### `server.hpp` / `server.cpp`

#### `SocketBase : SocketOptions`

Shared lifecycle for `Tcp` and `Udp`. Inherits `SocketOptions` so all socket configuration
methods are available on every server socket.

- **`init(IPType)`** — on Windows initializes Winsock (once per instance), then calls `::socket()`.
- **`bind(ip, port)`** — builds an `Address` and calls `::bind()`.
- **`closeSocket()`** — calls `platformClose()` and resets the handle to `invalidSocket`.
- **Destructor** — calls `closeSocket()` and `WSACleanup()` on Windows.

### Pause / Resume (`SocketBase`)
 
`SocketBase` has built-in pause/resume support for plain accept loops (without epoll).
State is tracked via `std::atomic<bool>` — no mutex or condition variable needed.
 
| Method | Description |
|---|---|
| `pause()` | Sets `isRunning_` to `false` and notifies all waiters. |
| `resume()` | Sets `isRunning_` to `true` and notifies all waiters. |
| `isRunning()` | Returns `true` if the server is active. |
| `waitUntilRunning()` | Blocks the calling thread until `resume()` is called. Uses `atomic::wait` — no busy spin. |
 
Usage in a plain accept loop:
```cpp
while (true) {
    if (!server.isRunning())
        server.waitUntilRunning();  // blocks until resume()
 
    auto client = server.accept();
    // ...
}
```
 
> **Note:** When using the epoll `Watcher`, pause/resume is controlled via `Watcher::pause()` /
> `Watcher::resume()` instead — see the epoll section below.
 
 
 
 
 
| Method | Description |
|---|---|
| `listen(backlog = 10)` | Marks the socket passive via `::listen()`. |
| `accept()` | Blocks until a client connects. Returns `unique_ptr<Connection>`. |
| `connect(ip, type, port)` | Connects to a remote endpoint. |
| `close()` | Explicit close. |
| `sendAll(span, totalBytes)` | Loops over `send()` until exactly `totalBytes` are sent. Returns `Error::BufferTooSmall` if the span is too small. |
| `receiveAll(span, totalBytes)` | Loops over `receive()` until exactly `totalBytes` are received. Returns `Error::BufferTooSmall` if the span is too small. |

#### `Udp : SocketBase`

| Method | Description |
|---|---|
| `sendTo(data, size, destination)` | Sends a datagram to a specific `Address`. |
| `receiveFrom(buffer, length)` | Reads a datagram and returns `{bytes, senderAddress}`. |

Both `Tcp` and `Udp` are non-copyable but movable.

---

### `epoll.hpp` / `epoll.cpp`

`Net::Poll::Watcher` is a thin, non-owning epoll-based event loop. Linux only.

Callbacks are registered once and fired on every matching event. All callbacks are **typed on your
concrete context type** — you never deal with `void*` or casts. The cast is hidden entirely.
---
 
### Pause / Resume (`Watcher`)
 
`Watcher` has independent pause/resume support for the epoll event loop. State is tracked via
`std::atomic<bool>` — no mutex or condition variable needed.
 
| Method | Description |
|---|---|
| `pause()` | Sets `paused_` to `true`. The event loop discards all pending events and blocks on the next iteration. |
| `resume()` | Sets `paused_` to `false` and notifies all waiters. The event loop resumes normally. |
| `isPaused()` | Returns `true` if the watcher is currently paused. |
 
Usage from another thread:
```cpp
// pause the event loop for 5 seconds
std::thread controller([&] {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    poller.pause();
 
    std::this_thread::sleep_for(std::chrono::seconds(5));
    poller.resume();
});
controller.detach();
 
poller.watch();  // blocks — respects pause/resume
```
 
> **Note:** `SocketBase::pause()` and `Watcher::pause()` are independent. When using
 
---



#### Context ownership — `Descriptor`

`Net::Poll::Descriptor` is the base struct every user defined context must publicly inherit from:
```cpp
struct Client : public Net::Poll::Descriptor {
    std::unique_ptr<Connection> conn;
    std::string name;
};
```

It carries a single member — `SocketHandle fd` — which you set before calling `add()`:
```cpp
auto client  = std::make_unique<Client>();
client->conn = std::move(conn.value());
client->fd   = client->conn->getSocket();  // ← required before add()

poller.add(client.get(), Net::EpollEvent::EPOLLIN);
```

The `Watcher` stores your pointer directly in epoll's `data.ptr` and recovers it on every
event, no extra allocation, no overhead.

**Ownership:** `Watcher` never owns or frees any pointer you pass in. You allocate it,
you free it — always inside `onClose`.

**Safety:** The `Watchable<T>` concept checks at compile time that `T` publicly inherits
`Descriptor`. Passing an unrelated type fails immediately with a clear error — nothing
unsafe ever reaches runtime:
```cpp
struct Random { int fd; };           // does not inherit Descriptor
poller.add(new Random{}, EPOLLIN);   //  compile error: Watchable<Random> not satisfied

struct Client : public Net::Poll::Descriptor { ... };
poller.add(new Client{}, EPOLLIN);   // will work
```

---

#### Trigger modes

**Level-triggered** (default): `epoll_wait` keeps firing as long as data is available. Safe to
read partially — you will be notified again next iteration.

**Edge-triggered** (`EPOLLET`): `epoll_wait` fires once when new data arrives. You must drain the
socket completely in one go or you will miss events until the next write.

---

#### Methods

| Method | Description |
|---|---|
| `Watcher::create(timeout)` | Factory. `timeout` in ms: `0` = non-blocking, `-1` = block forever, `>0` = fire `onTimeout` after N ms. |
| `add<T>(desc, events)` | Registers a `Descriptor*`. `Watcher` never owns or frees it. |
| `mod<T>(desc, events)` | Replaces the event mask for an already-registered descriptor. Must re-supply `desc` — `MOD` overwrites the entire registration. |
| `close<T>(desc)` | Removes descriptor from epoll then fires `onClose`. Always go through here — skipping it leaks the context. |
| `watch()` | Blocks forever dispatching callbacks. `EINTR` is retried silently. Returns only on a fatal `epoll_wait` error. |

---

#### Callbacks

All callbacks are **templated on your concrete type T** — you receive `T*` directly.
The `void* → Descriptor* → T*` cast chain is handled by the library and never appears in user code.

| Setter | Fired when |
|---|---|
| `onRead<T>(cb)` | fd has data ready to read (`EPOLLIN`) |
| `onWrite<T>(cb)` | fd is ready to write (`EPOLLOUT`) |
| `onError<T>(cb)` | `EPOLLERR` or `EPOLLHUP` — call `close()` from here |
| `onClose<T>(cb)` | After `close()` removes the fd, or when the peer shuts down (`EPOLLRDHUP`). **You must reclaim your context here or it leaks.** |
| `onTimeout(cb)` | `epoll_wait` timed out — use for idle work, heartbeats, expiring stale connections |


---

#### Example

**Setup:**
```cpp
auto server = Net::Servers::Tcp{};
server.init(Net::IPType::IPv4);
server.setNonBlocking();
server.setReusePort();
server.setReuseAddress();
server.bind("0.0.0.0", 8080);
server.listen();

auto poll = Net::Poll::Watcher::create(3000);
Net::Poll::Watcher& poller = poll.value();

// Server socket has no client state  bare Descriptor is enough
Net::Poll::Descriptor serverDesc{ server.getSocket() };
poller.add(&serverDesc, Net::EpollEvent::EPOLLIN | Net::EpollEvent::EPOLLERR);
```

**Context struct:**
```cpp
struct Client : public Net::Poll::Descriptor {
    std::unique_ptr<Connection>         conn;
    std::array<uint8_t, 4096>           buffer{};
    size_t totalLen = 0;
    size_t offset   = 0;
};

// map is the sole owner — poller holds raw observer pointers only
std::unordered_map<int, std::unique_ptr<Client>> clients;
```

**Accept → Read → Write → Close pipeline:**
```cpp
poller.onClose<Client>([&](Client* client) {
    clients.erase(client->fd);  // unique_ptr destructor frees Client
});

poller.onRead<Net::Poll::Descriptor>([&](Net::Poll::Descriptor* desc) {
    if (desc->fd == serverDesc.fd) {
        // server fd — accept new client
        auto conn    = server.accept();
        auto client  = std::make_unique<Client>();
        client->conn = std::move(conn.value());
        client->fd   = client->conn->getSocket();  // set Descriptor::fd
        poller.add(client.get(), Net::EpollEvent::EPOLLIN);
        clients.emplace(client->fd, std::move(client));
    } else {
        // client fd — read request, arm EPOLLOUT to send response
        auto* client = static_cast<Client*>(desc);
        client->conn->receive(client->buffer.data(), client->buffer.size());
        poller.mod(client, Net::EpollEvent::EPOLLOUT);
    }
});

poller.onWrite<Client>([&](Client* client) {
    client->conn->send(client->buffer.data() + client->offset,
                       client->totalLen      - client->offset);
    client->offset += sent.value();
    if (client->offset >= client->totalLen)
        poller.close(client);  // triggers onClose → clients.erase → Client freed
});

poller.onError<Client>([&](Client* client, Net::Error err) {
    std::println("error fd={}: {}", client->fd, Net::toErrorString(err));
    poller.close(client);
});

poller.onTimeout([&]() {
    // idle work — ping clients, expire stale connections, etc.
});

poller.watch();
```

> **Note on `onRead` type parameter:** If a single `onRead` callback handles both the server
> socket and client sockets, use `Net::Poll::Descriptor` as the type parameter and downcast
> manually in the client branch with `static_cast<Client*>(desc)`. This is the only place
> a cast appears in user code, and only because two distinct descriptor types share one callback.

#### Run
```cpp
poller.watch(); // blocks — fires callbacks on events
```

---

## Error Handling

No exceptions anywhere. Every fallible call returns `Result<T>`. Check with `if (result)` and
read the error with `result.error()`:

```cpp
auto server = Net::Servers::Tcp{};

if (auto r = server.init(Net::IPType::IPv4); !r)
    std::println("init failed: {}", Net::toErrorString(r.error()));

if (auto r = server.bind("0.0.0.0", 8080); !r)
    std::println("bind failed: {}", Net::toErrorString(r.error()));

server.listen();

while (true) {
    auto client = server.accept();
    if (!client) break;

    client.value()->setReceiveTimeout(std::chrono::milliseconds(2000));
    client.value()->setSendTimeout(std::chrono::milliseconds(2000));

    std::array<uint8_t, 1024> buf{};
    auto received = client.value()->receive(buf.data(), buf.size());
    if (!received) {
        std::println("recv error: {}", Net::toErrorString(received.error()));
        continue;
    }

    std::string_view request(reinterpret_cast<char*>(buf.data()), received.value());
    std::println("request: {}", request);
}
```

---

## Requirements

- C++23
- Windows (tested) / Linux (not tested for the timeout)
- `epoll.hpp` / `epoll.cpp` — Linux only
