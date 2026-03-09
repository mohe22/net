#include "../include/server.hpp"
#include "../include/epoll.hpp"
#include <array>
#include <memory>
#include <print>
#include <string_view>
#include <cstring>
#include <unordered_map>

using namespace Net;

namespace Net::Http {

    enum class StatusCode : uint16_t {
        Ok                  = 200,
        Created             = 201,
        NoContent           = 204,
        BadRequest          = 400,
        Unauthorized        = 401,
        Forbidden           = 403,
        NotFound            = 404,
        MethodNotAllowed    = 405,
        InternalServerError = 500,
        NotImplemented      = 501,
        ServiceUnavailable  = 503,
    };

    std::string makeResponse(StatusCode code, std::string_view body) {
        std::string_view statusLine;
        switch (code) {
            case StatusCode::Ok:       statusLine = "HTTP/1.1 200 OK";                   break;
            case StatusCode::NotFound: statusLine = "HTTP/1.1 404 Not Found";            break;
            default:                   statusLine = "HTTP/1.1 500 Internal Server Error"; break;
        }
        return std::string(statusLine)
             + "\r\nContent-Type: text/plain"
             + "\r\nContent-Length: " + std::to_string(body.size())
             + "\r\nConnection: close"
             + "\r\n\r\n"
             + std::string(body);
    }

} // namespace Net::Http

constexpr size_t WRITE_BUF_SIZE = 4096;

struct Client : public Net::Poll::Descriptor {
    std::unique_ptr<Connection>         conn;
    std::array<uint8_t, WRITE_BUF_SIZE> buffer{};
    size_t totalLen = 0;
    size_t offset   = 0;
};

int main() {
    auto server = Net::Servers::Tcp{};

    if (auto r = server.init(Net::IPType::IPv4); !r) {
        std::println("init failed: {}", Net::toErrorString(r.error())); return 1;
    }
    if (auto r = server.setNonBlocking(); !r) {
        std::println("setNonBlocking failed: {}", Net::toErrorString(r.error())); return 1;
    }
    if (auto r = server.setReusePort(); !r) {
        std::println("setReusePort failed: {}", Net::toErrorString(r.error())); return 1;
    }
    if (auto r = server.setReuseAddress(); !r) {
        std::println("setReuseAddress failed: {}", Net::toErrorString(r.error())); return 1;
    }
    if (auto r = server.bind("0.0.0.0", 8080); !r) {
        std::println("bind failed: {}", Net::toErrorString(r.error())); return 1;
    }
    if (auto r = server.listen(); !r) {
        std::println("listen failed: {}", Net::toErrorString(r.error())); return 1;
    }

    auto poll = Net::Poll::Watcher::create();
    if (!poll) return 1;
    Net::Poll::Watcher& poller = poll.value();

    Net::Poll::Descriptor serverDesc{ server.getSocket() };

    std::unordered_map<int, std::unique_ptr<Client>> clients;

    if (auto r = poller.add(&serverDesc,
            Net::EpollEvent::EPOLLIN | Net::EpollEvent::EPOLLERR | Net::EpollEvent::EPOLLHUP); !r) {
        std::println("add server failed: {}", Net::toErrorString(r.error())); return 1;
    }

    poller.onClose<Client>([&](Client* client) {
        std::println("client fd={} disconnected", client->fd);
        clients.erase(client->fd);
    });

    poller.onTimeout([&]() {
        std::println("timeout — checking stale connections");
    });

    poller.onRead<Net::Poll::Descriptor>([&](Net::Poll::Descriptor* desc) {
        if (desc->fd == serverDesc.fd) {
            auto conn = server.accept();
            if (!conn) {
                std::println("accept error: {}", Net::toErrorString(conn.error()));
                return;
            }

            auto [ip, port] = conn.value()->getRemoteIpPort();
            std::println("new client {}:{}", ip, port);

            if (auto r = conn.value()->setNonBlocking(); !r) {
                std::println("setNonBlocking failed: {}", Net::toErrorString(r.error()));
                return;
            }

            auto client  = std::make_unique<Client>();
            client->conn = std::move(conn.value());
            client->fd   = client->conn->getSocket();

            if (auto r = poller.add(client.get(),
                    Net::EpollEvent::EPOLLIN | Net::EpollEvent::EPOLLERR | Net::EpollEvent::EPOLLHUP); !r) {
                std::println("add client failed: {}", Net::toErrorString(r.error()));
                return;
            }

            clients.emplace(client->fd, std::move(client));
            return;
        }

        auto* client = static_cast<Client*>(desc);

        std::array<uint8_t, 4096> buf{};
        auto received = client->conn->receive(buf.data(), buf.size());

        if (!received || received.value() == 0) {
            poller.close(client);
            return;
        }

        std::string_view request(reinterpret_cast<char*>(buf.data()), received.value());
        auto firstLine = request.substr(0, request.find("\r\n"));

        std::string_view path = "/";
        auto pStart = firstLine.find(' ');
        auto pEnd   = firstLine.rfind(' ');
        if (pStart != std::string_view::npos && pEnd != std::string_view::npos)
            path = firstLine.substr(pStart + 1, pEnd - pStart - 1);

        auto response = (path == "/")
            ? Net::Http::makeResponse(Net::Http::StatusCode::Ok,      "Hello from Net!\n")
            : Net::Http::makeResponse(Net::Http::StatusCode::NotFound, "404 Not Found\n");

        if (response.size() > WRITE_BUF_SIZE) {
            std::println("response too large for fd={}", client->fd);
            poller.close(client);
            return;
        }

        std::memcpy(client->buffer.data(), response.data(), response.size());
        client->totalLen = response.size();
        client->offset   = 0;

        if (auto r = poller.mod(client,
                Net::EpollEvent::EPOLLOUT | Net::EpollEvent::EPOLLERR | Net::EpollEvent::EPOLLHUP); !r) {
            std::println("mod failed fd={}: {}", client->fd, Net::toErrorString(r.error()));
            poller.close(client);
        }
    });

    poller.onWrite<Client>([&](Client* client) {
        auto sent = client->conn->send(
            client->buffer.data() + client->offset,
            client->totalLen      - client->offset
        );

        if (!sent) {
            std::println("send error fd={}: {}", client->fd, Net::toErrorString(sent.error()));
            poller.close(client);
            return;
        }

        client->offset += sent.value();

        if (client->offset >= client->totalLen)
            poller.close(client);
    });

    poller.onError<Client>([&](Client* client, Net::Error err) {
        std::println("error on fd={}: {}", client->fd, Net::toErrorString(err));
        poller.close(client);
    });

    std::println("Listening on :8080");

    if (auto r = poller.watch(); !r) {
        std::println("watch error: {}", Net::toErrorString(r.error()));
        return 1;
    }

    return 0;
}
