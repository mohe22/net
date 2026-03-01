#include "../include/server.hpp"
#include "../include/client.hpp"
#include "../include/types.hpp"
int main() {

    Net::Servers::Tcp server;
    auto result = server.init(Net::IPType::IPv4)
        .and_then([&]() -> Net::Result<void> {
            return server.bind("127.0.0.1", 8080);
        })
        .and_then([&]() -> Net::Result<void> {
            return server.listen();
        })
        .and_then([&]() -> Net::Result<void> {
            uint8_t buffer[1204];
            while (true) {
                Net::Result<std::unique_ptr<Net::Client>> client = server.accept();
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
