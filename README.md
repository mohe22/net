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
│   └── net.hpp            # Top-level convenience include
└── src/
    ├── address.cpp        # Address factory implementations
    ├── connection.cpp     # Connection send/receive/close
    ├── socketOptions.cpp  # setOption / setTimeoutOption implementations
    ├── server.cpp         # SocketBase init/bind, Tcp listen/accept, Udp sendTo/receiveFrom
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
`Result<T>`. Raw accessors `getAddrRaw()` and `getSizeRaw()` expose the underlying storage for
OS calls like `bind()`, `connect()`, etc.

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

#### `Tcp : SocketBase`

| Method | Description |
|---|---|
| `listen(backlog = 10)` | Marks the socket passive via `::listen()`. |
| `accept()` | Blocks until a client connects. Returns `unique_ptr<Connection>`. |
| `connect(ip, type, port)` | Connects to a remote endpoint. |
| `close()` | Explicit close. |

#### `Udp : SocketBase`

| Method | Description |
|---|---|
| `sendTo(data, size, destination)` | Sends a datagram to a specific `Address`. |
| `receiveFrom(buffer, length)` | Reads a datagram and returns `{bytes, senderAddress}`. |

Both `Tcp` and `Udp` are non-copyable but movable.

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
