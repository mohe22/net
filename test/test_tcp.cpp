#include "../include/server.hpp"
#include "../include/connection.hpp"
#include "../include/types.hpp"
#include <print>
#include <thread>

int main() {
    Net::Servers::Tcp server;

    std::thread controller([&]() {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::println("pausing server...");
        server.pause();
        std::println("isPaused: {}", server.isPaused());

        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::println("resuming server...");
        server.resume();
        std::println("isPaused: {}", server.isPaused()); // → false

        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::println("stopping server...");
        server.stop();
        std::println("isStopped: {}", server.isStopped());
    });
    controller.detach();

    auto result = server.init(Net::IPType::IPv4)
        .and_then([&]() -> Net::Result<void> {
            return server.bind("127.0.0.1", 8083);
        })
        .and_then([&]() -> Net::Result<void> {
            return server.listen();
        })
        .and_then([&]() -> Net::Result<void> {
            std::chrono::milliseconds timeout{6000};
            if (auto r = server.setSendTimeout(timeout);    !r) std::println("send timeout failed: {}",    Net::toErrorString(r.error()));
            if (auto r = server.setReceiveTimeout(timeout); !r) std::println("receive timeout failed: {}", Net::toErrorString(r.error()));
            if (auto r = server.setReusePort();             !r) std::println("reuse port failed: {}",       Net::toErrorString(r.error()));
            if (auto r = server.setReuseAddress();          !r) std::println("reuse address failed: {}",    Net::toErrorString(r.error()));
            if (auto r = server.setKeepAlive();             !r) std::println("keep alive failed: {}",       Net::toErrorString(r.error()));
            return {};
        })
        .and_then([&]() -> Net::Result<void> {
            uint8_t buffer[1204];

            while (true) {
                if (server.isPaused()) {
                    std::println("server paused — waiting for resume...");
                    server.waitUntilRunning();
                    std::println("server resumed");
                }

                if (server.isStopped()) {
                    std::println("server stopped — exiting accept loop");
                    break;
                }
                std::println("waiting for client...");
                Net::Result<std::unique_ptr<Net::Connection>> client = server.accept();
                if (!client) {
                    if (server.isStopped()) break;
                    std::println("[accept] error: {}", Net::toErrorString(client.error()));
                    continue;
                }

                auto [ip, port] = client.value()->getRemoteIpPort();
                std::println("[accept] new connection from {}:{}", ip, port);

                Net::Result<int> bytesReceived = client.value()->receive(buffer, sizeof(buffer));
                if (!bytesReceived) {
                    std::println("[receive] error: {}", Net::toErrorString(bytesReceived.error()));
                    continue;
                }
                std::println("[receive] {} bytes", bytesReceived.value());

                const char response[] =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 13\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "Hello there!!!";

                auto sendBytes = client.value()->send(response, sizeof(response) - 1);
                if (!sendBytes) {
                    std::println("[send] error: {}", Net::toErrorString(sendBytes.error()));
                    continue;
                }
                std::println("[send] {} bytes", sendBytes.value());
            }

            return {};
        })
        .or_else([](const auto& error) -> Net::Result<void> {
            std::println("[fatal] {}", Net::toErrorString(error));
            return {};
        });

    return 0;
}
