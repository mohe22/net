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
        server.pause();   //  sets isRunning_ to false

        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::println("resuming server...");
        server.resume();  // sets isRunning_ to true
    });
    controller.detach();
    auto result = server.init(Net::IPType::IPv4)

        .and_then([&]() -> Net::Result<void> {
            return server.bind("127.0.0.1", 8083);
        })
        .and_then([&]() -> Net::Result<void> {
            return server.listen();
        })
        .and_then([&]()->Net::Result<void>{
            std::chrono::milliseconds timeout{6000};
            if (auto r = server.setSendTimeout(timeout); !r)
                std::println("failed timeout: {}", Net::toErrorString(r.error()));
            if (auto r = server.setReceiveTimeout(timeout); !r)
                std::println("failed receive timeout: {}", Net::toErrorString(r.error()));
            if (auto r = server.setReusePort(); !r)
                std::println("failed reuse port: {}", Net::toErrorString(r.error()));
            if(auto r = server.setReuseAddress(); !r)
                std::println("failed reuse address: {}", Net::toErrorString(r.error()));
            if(auto r = server.setKeepAlive(); !r)
                std::println("failed keep alive: {}", Net::toErrorString(r.error()));
            return {};
        })
        .and_then([&]() -> Net::Result<void> {
            uint8_t buffer[1204];
            while (true) {

                if (!server.isRunning())  {
                      std::println("server paused — waiting...");
                      server.waitUntilRunning();
                      std::println("server resumed");
                  }

                Net::Result<std::unique_ptr<Net::Connection>> client = server.accept();
                if (!client) {

                    std::println("[DEBUG] Accept error: {}", Net::toErrorString(client.error()));
                    continue;
                }
                Net::Result<int> bytesReceived = client.value()->receive(buffer, sizeof(buffer));
                if(!bytesReceived) {
                    std::println("[DEBUG] Receive error: {}", Net::toErrorString(bytesReceived.error()));
                    continue;
                }
                std::println("Received: {} bytes",bytesReceived.value());


                const char response[] =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 13\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "Hello there!!!";

                auto sendBytes = client.value()->send(response, sizeof(response) - 1);
                if(!sendBytes) {
                    std::println("[DEBUG] send error: {}", Net::toErrorString(sendBytes.error()));
                    continue;
                }
            }
            return {};
        })
        .or_else([](const auto& error) -> Net::Result<void> {
            std::println("[DEBUG]: Error  {}", Net::toErrorString(error));
            return {};
        });

    return 0;
}
