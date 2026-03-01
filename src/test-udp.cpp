#include "../include/server.hpp"
#include "../include/types.hpp"
#include <print>

void testIpv6(){
    Net::Servers::Udp server;
    std::string errrorOcurred{"MAIN"};
    auto res= server.init(Net::IPType::IPv6)
        .and_then([&]() -> Net::Result<void> {
            errrorOcurred = "init failed";
            return server.bind("::",8080);
        }).and_then([&]()->Net::Result<void> {
            errrorOcurred = "init failed";
            uint8_t buffer[1024];
            const char* message{"Hello, World!"};
            while(true){
                Net::Result<Net::RecvFromResult> result = server.receiveFrom(buffer, sizeof(buffer));
                if (!result) {
                    std::println("[ERROR] receiveFrom failed: {}", Net::toErrorString(result.error()));
                    continue; // or break/return depending on your loop
                }

                auto [recvBytes, receiver] = result.value();
                std::println("[DEBUG] received: {} (Bytes), address: {}:{}",
                    recvBytes,
                    receiver.getIp().value_or("Unknown"),
                    receiver.getPort().value_or(0)
                );

                Net::Result<ssize> sentBytes = server.sendTo(message, strlen(message), receiver);
                if (!sentBytes) {
                    std::println("[ERROR] sendTo failed: {}", Net::toErrorString(sentBytes.error()));
                    continue;
                }

                std::println("[DEBUG] sent: {} (Bytes)", sentBytes.value());
           }

           return {};
        })
        .or_else([&](const Net::Error& error) -> Net::Result<void> {
            std::println("[DEBUG]: Error  {}, {}", Net::toErrorString(error), errrorOcurred);

            return std::unexpected(error);
    });

}
void testIpv4(){
    Net::Servers::Udp server;
    std::string errrorOcurred{"MAIN"};
    auto res= server.init(Net::IPType::IPv4)
        .and_then([&]()->Net::Result<void>{
            return server.bind("0.0.0.0",8080);
        })
        .and_then([&]()->Net::Result<void> {
            errrorOcurred = "init failed";
            uint8_t buffer[1024];
            const char* message{"Hello, World!"};
            while(true){
                Net::Result<Net::RecvFromResult> result = server.receiveFrom(buffer, sizeof(buffer));
                if (!result) {
                    std::println("[ERROR] receiveFrom failed: {}", Net::toErrorString(result.error()));
                    continue;
                }

                auto [recvBytes, receiver] = result.value();
                std::println("[DEBUG] received: {} (Bytes), address: {}:{}",
                    recvBytes,
                    receiver.getIp().value_or("Unknown"),
                    receiver.getPort().value_or(0)
                );

                Net::Result<ssize> sentBytes = server.sendTo(message, strlen(message), receiver);
                if (!sentBytes) {
                    std::println("[ERROR] sendTo failed: {}", Net::toErrorString(sentBytes.error()));
                    continue;
                }

                std::println("[DEBUG] sent: {} (Bytes)", sentBytes.value());
           }

           return {};
        })
        .or_else([&](const Net::Error& error) -> Net::Result<void> {
            std::println("[DEBUG]: Error  {}, {}", Net::toErrorString(error), errrorOcurred);

            return std::unexpected(error);
    });
}

int main(){

    testIpv4();
    return 0;

};
