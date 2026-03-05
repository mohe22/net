#include "../include/client.hpp"
#include "../include/types.hpp"
#include <fstream>
#include <print>

using namespace std;
using namespace Net;

int main()
{
    const string filePath = "./received.bin";

    auto clientResult = Client::connect("127.0.0.1", 8080, IPType::IPv4);
    if (!clientResult)
    {
        std::println("Failed to connect to server: {}", Net::toErrorString(clientResult.error()));
        return 1;
    }

    auto &client = clientResult.value();
    std::println("Connected to server!");

    std::fstream file(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file)
    {
        std::println("Failed to open file for writing: {}", filePath);
        return 1;
    }

    char buffer[1024];
    Net::ssize totalReceived = 0;

    while (true)
    {
        auto result = client->receive(buffer, sizeof(buffer));
        if (!result)
        {
            if (result.error() == Net::Error::ConnectionClosed)
            {
                std::println("Server closed the connection. Total bytes received: {}", totalReceived);
                break;
            }
            std::println("Failed to receive data: {}", Net::toErrorString(result.error()));
            file.close();
            return 1;
        }

        Net::ssize bytesReceived = result.value();
        file.write(buffer, bytesReceived);

        if (!file)
        {
            std::println("Failed to write to file after {} bytes", totalReceived);
            file.close();
            return 1;
        }

        totalReceived += bytesReceived;
        std::println("Received {} bytes ({} total)", bytesReceived, totalReceived);
    }

    file.close();
    std::println("File saved to: {}", filePath);
    return 0;
}