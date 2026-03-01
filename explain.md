# Net — C++ Cross-Platform Socket Library

A lightweight, modern C++ networking library built on top of raw POSIX and Winsock2 sockets.
It provides a clean, type-safe API using `std::expected` for error handling instead of exceptions,
and supports both TCP and UDP over IPv4 and IPv6.

---

## Project Structure

```
net/
├── include/
│   ├── platform.hpp   # Platform abstraction (Winsock2 vs POSIX)
│   ├── error.hpp      # Error enum + getError() + toErrorString()
│   ├── types.hpp      # Result<T>, IPType, Protocol, helper converters
│   ├── address.hpp    # Address class (IPv4/IPv6 wrapper over sockaddr_storage)
│   ├── client.hpp     # Client class (owns a connected TCP socket)
│   ├── server.hpp     # SocketBase, Tcp, Udp server classes
│   └── net.hpp        # (Top-level convenience include)
└── src/
    ├── address.cpp    # Address factory implementations
    ├── client.cpp     # Client send/receive/close
    ├── server.cpp     # SocketBase init/bind, Tcp listen/accept, Udp sendTo/receiveFrom
    ├── test-udp.cpp   # UDP usage example / test
    └── test_tcp.cpp   # TCP usage example / test
```

---

## Architecture

The library is structured in layers, each building on the one below it.

```
┌─────────────────────────────────────┐
│         Tcp / Udp (server.hpp)      │  ← Protocol-specific server logic
├─────────────────────────────────────┤
│         SocketBase (server.hpp)     │  ← Shared socket lifecycle (init, bind, close)
├─────────────────────────────────────┤
│         Client (client.hpp)         │  ← Owns an accepted/connected socket
├─────────────────────────────────────┤
│         Address (address.hpp)       │  ← Wraps sockaddr_storage (IPv4 + IPv6)
├─────────────────────────────────────┤
│         types.hpp / error.hpp       │  ← Result<T>, Error, IPType, Protocol
├─────────────────────────────────────┤
│         platform.hpp                │  ← SocketHandle, ssize, getLastError(), platformClose()
└─────────────────────────────────────┘
```

### `platform.hpp` — Platform Abstraction

The foundation of the library. Conditionally includes either `<winsock2.h>` (Windows)
or the standard POSIX socket headers, and normalizes the differences behind a common interface:

| Abstraction        | Windows              | POSIX          |
|--------------------|----------------------|----------------|
| `SocketHandle`     | `SOCKET`             | `int`          |
| `invalidSocket`    | `INVALID_SOCKET`     | `-1`           |
| `ssize`            | `int`                | `ssize_t`      |
| `getLastError()`   | `WSAGetLastError()`  | `errno`        |
| `platformClose(s)` | `::closesocket(s)`   | `::close(s)`   |

No other file in the library contains `#ifdef _WIN32` for these concerns — they all go through `platform.hpp`.

---

### `error.hpp` — Error Handling

Defines the `Net::Error` enum (`uint8_t`) covering every failure category the library can produce,
grouped by operation: initialization, address/IP, bind, listen, accept, connect, send, receive, close, and socket state.

Two inline utilities accompany it:

- **`getError()`** — reads `getLastError()` and maps the raw platform code to a `Net::Error`.
- **`toErrorString(Error)`** — maps any `Net::Error` to a human-readable `std::string_view`.

---

### `types.hpp` — Core Types and Converters

Defines the types and free functions used across the entire library:

- **`Result<T>`** — alias for `std::expected<T, Net::Error>`. Every function that can fail returns this instead of throwing.
- **`RecvFromResult`** — `std::tuple<ssize, Address>` returned by `Udp::receiveFrom()`.
- **`Protocol`** — `TCP`, `UDP`, `RAW` enum.
- **`IPType`** — `IPv4`, `IPv6` enum.
- **`toAddressFamily(IPType)`** — maps `IPType` → `AF_INET` / `AF_INET6`.
- **`toSocketType(Protocol)`** — maps `Protocol` → `SOCK_STREAM` / `SOCK_DGRAM` / `SOCK_RAW`.
- **`fromAddressFamily(int)`** — reverse maps `AF_*` → `IPType`.
- **`getAddressSizeByIpType(IPType)`** — returns `sizeof(sockaddr_in)` or `sizeof(sockaddr_in6)`.

---

### `address.hpp` / `address.cpp` — Network Address

`Address` wraps a `sockaddr_storage`, which is large enough to hold either an IPv4 (`sockaddr_in`)
or IPv6 (`sockaddr_in6`) address. It is constructed exclusively through static factory methods:

| Factory | Source |
|---|---|
| `Address::from(string, port)` | Human-readable IP string + port — tries IPv4 then IPv6 via `inet_pton` |
| `Address::from(sockaddr_storage&)` | OS-provided storage — dispatches by `ss_family` |
| `Address::from(sockaddr_in&)` | Pre-populated IPv4 struct |
| `Address::from(sockaddr_in6&)` | Pre-populated IPv6 struct |

Accessors (`getIp()`, `getPort()`, `getIpType()`) return `Result<T>` and will return an error
if the address has not been initialized. Raw pointer accessors (`getAddrRaw()`, `getSizeRaw()`)
are provided for passing directly into OS socket calls.

---

### `client.hpp` / `client.cpp` — Connected TCP Client

`Client` represents one side of an established TCP connection. It owns a `SocketHandle` and
the remote `Address`. Instances are created by `Tcp::accept()` and handed back as
`std::unique_ptr<Client>` — the caller owns the lifetime.

`Client` is non-copyable and non-movable by design: a socket is a unique OS resource and
should not be shared or silently transferred.

| Method | Description |
|---|---|
| `send(data, size)` | Calls `::send()`. Returns bytes sent or an error. |
| `receive(data, size)` | Calls `::recv()`. Returns bytes received or an error. |
| `close()` | Explicitly closes the socket. The destructor also closes if not already done. |

---

### `server.hpp` / `server.cpp` — Server Socket Classes

#### `SocketBase`

Abstract base providing the shared lifecycle for both `Tcp` and `Udp`:

- **`init(IPType)`** — initializes Winsock on Windows (once per instance), then calls `::socket()` using the address family resolved from `IPType` and the `SOCK_*` constant from the subclass via the pure virtual `socketType()`.
- **`bind(ip, port)`** — constructs an `Address` and calls `::bind()`.
- **`closeSocket()`** — calls `platformClose()` and resets the handle.
- **Destructor** — calls `closeSocket()` and `WSACleanup()` on Windows.

#### `Tcp : SocketBase`

Adds connection-oriented operations:

| Method | Description |
|---|---|
| `listen(backlog)` | Marks the socket passive via `::listen()`. |
| `accept()` | Blocks until a client connects; returns `unique_ptr<Client>`. |
| `connect(ip, type, port)` | Client-side connection to a remote endpoint. |
| `close()` | Explicit close (separate from destructor). |

#### `Udp : SocketBase`

Adds connectionless datagram operations:

| Method | Description |
|---|---|
| `sendTo(data, size, destination)` | Sends a datagram to a specific `Address` via `::sendto()`. |
| `receiveFrom(buffer, length)` | Reads a datagram and returns the sender's `Address` via `::recvfrom()`. |

Both `Tcp` and `Udp` are non-copyable but movable.

---

## Error Handling Philosophy

No exceptions are thrown anywhere in the library. Every operation that can fail returns
`Result<T>` (`std::expected<T, Net::Error>`). The caller checks success with `if (result)`
and retrieves the error with `result.error()`:

```cpp
auto server = Net::Servers::Tcp{};
if (auto r = server.init(Net::IPType::IPv4); !r)
    std::println("init failed: {}", Net::toErrorString(r.error()));

if (auto r = server.bind("0.0.0.0", 8080); !r)
    std::println("bind failed: {}", Net::toErrorString(r.error()));

server.listen();

while (true) {
    auto client = server.accept();
    if (!client)
        break;

    std::array<uint8_t, 1024> buf{};
    auto received = client.value()->receive(buf.data(), buf.size());
    if (!received)
        std::println("recv error: {}", Net::toErrorString(received.error()));
}
```

---

## Platform Support
1. Linux (Not tested yet).
2. Windows.


Requires C++23 for `std::expected` and `std::println`.
