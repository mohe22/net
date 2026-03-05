#include "../include/server.hpp"
#include "../include/client.hpp"
#include "../include/types.hpp"
#include <fstream>
#include <print>

using namespace std;
using namespace Net;

int main()
{
    string filePath = "./file.bin";

    Servers::Tcp server;
    if (auto r = server.init(IPType::IPv4); !r)
    {
        std::println("Failed to initialize server: {}", Net::toErrorString(r.error()));
        return 1;
    }
    if (auto r = server.setReusePort(); !r)
    {
        std::println("Failed to set reuse port: {}", Net::toErrorString(r.error()));
        return 1;
    }
    if (auto r = server.setReuseAddress(); !r)
    {
        std::println("Failed to set reuse address: {}", Net::toErrorString(r.error()));
        return 1;
    }
    if (auto r = server.bind("0.0.0.0", 8080); !r)
    {
        std::println("Failed to bind server: {}", Net::toErrorString(r.error()));
        return 1;
    }
    if (auto r = server.listen(); !r)
    {
        std::println("Failed to listen: {}", Net::toErrorString(r.error()));
        return 1;
    }

    auto client = server.accept();
    if (!client)
    {
        std::println("Failed to accept client: {}", Net::toErrorString(client.error()));
        return 1;
    }

    std::fstream file(filePath, std::ios::in | std::ios::binary);
    if (!file)
    {
        std::println("Failed to open file: {}", filePath);
        return 1;
    }

    file.seekg(0, std::ios::end);
    std::size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::println("Total size: {} bytes", fileSize);

    char buffer[1024];
    std::size_t bytesSent = 0;

    while (bytesSent < fileSize)
    {
        std::size_t toRead = std::min(sizeof(buffer), fileSize - bytesSent);

        file.read(buffer, toRead);
        std::size_t bytesRead = file.gcount();
        if (bytesRead == 0)
        {
            // std::println("Unexpected end of file at {} bytes", bytesSent);
            file.close();
            return 1;
        }

        auto result = client.value()->send(buffer, bytesRead);
        if (!result)
        {
            // std::println("Failed to send data: {}", Net::toErrorString(result.error()));
            file.close();
            return 1;
        }

        if ((size_t)result.value() != bytesRead)
        {
            std::println("Partial send: sent {}/{} bytes — transfer corrupted", result.value(), bytesRead);
            file.close();
            return 1;
        }

        bytesSent += (size_t)result.value();
        // std::println("Sent {}/{} bytes", bytesSent, fileSize);
    }

    std::println("Transfer complete: {} bytes sent", bytesSent);
    file.close();
    return 0;
}