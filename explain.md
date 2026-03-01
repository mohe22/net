
net/
├── platform.h        ← all platform ifdefs live here, nowhere else
├── error.h           ← Error enum, fromErrno(), toErrorString()
├── types.h           ← Result, Protocol, IPType, SocketHandle
├── address.h/.cpp    ← IPAddress, Endpoint (ip + port)
├── socket.h/.cpp     ← Socket (raw RAII handle, send/recv/close)
├── server.h/.cpp     ← Server (bind, listen, accept)
├── client.h/.cpp     ← Client (connect, send, recv)
└── net.h             ← single include that pulls everything
