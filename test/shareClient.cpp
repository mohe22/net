

#include "../include/server.hpp"
#include "../include/client.hpp"
#include "../include/types.hpp"
#include <fstream>

#include <print>

using namespace std;
using namespace Net;

int main()
{

    // g++ ../src/socketOptions.cpp ../src/client.cpp ../src/address.cpp ../src/server.cpp http.cpp --std=c++26 -fsanitize=address,undefined -Wall -Wextra  -o server-http

    Net::Client client;
    if (auto r = client.connect("127.0.0.1",IPType::IPv4, 8080); !r)
    {
        std::println("Failed to connect to server: {}", Net::toErrorString(r.error()));
        return 1;
    }

    if (auto r = server.init(IPType::IPv4); !r)
    {
        std::println("Failed to initialize server: {}", Net::toErrorString(r.error()));
        return 1;
    }
    if (auto r = server.bind("0.0.0.0:8080", 8080); !r)
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

    std::fstream file(filePath, std::ios::in | std::ios::out | std::ios::binary);

    if (!file)
    {
        std::println("Failed to create file: {}", filePath);
        return 1;
    }

    char buffer[1024];
    Net::ssize bytesSent = 0;

    file.seekg(0, std::ios::end);
    std::size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    while (bytesSent < fileSize)
    {
        size_t toRead = std::min(sizeof(buffer), fileSize - bytesSent);
        file.read(buffer, toRead);
        auto bytesSend = client.value()->send(
            buffer + bytesSent,
            toRead);
        if (!bytesSend)
        {
            std::println("Failed to send data: {}", Net::toErrorString(bytesSend.error()));
            file.close();
            return 1;
        }
        bytesSent += bytesSend.value();
    }

    file.close();
}
