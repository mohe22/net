
#include "../include/server.hpp"

#include <chrono>
#include <print>
#include <string_view>

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

    inline std::string_view toResponse(StatusCode code) noexcept {
        switch (code) {
            case StatusCode::Ok:
                return "HTTP/1.1 200 OK\r\n\r\n";
            case StatusCode::Created:
                return "HTTP/1.1 201 Created\r\n\r\n";
            case StatusCode::NoContent:
                return "HTTP/1.1 204 No Content\r\n\r\n";
            case StatusCode::BadRequest:
                return "HTTP/1.1 400 Bad Request\r\n\r\n";
            case StatusCode::Unauthorized:
                return "HTTP/1.1 401 Unauthorized\r\n\r\n";
            case StatusCode::Forbidden:
                return "HTTP/1.1 403 Forbidden\r\n\r\n";
            case StatusCode::NotFound:
                return "HTTP/1.1 404 Not Found\r\n\r\n";
            case StatusCode::MethodNotAllowed:
                return "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
            case StatusCode::InternalServerError:
                return "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            case StatusCode::NotImplemented:
                return "HTTP/1.1 501 Not Implemented\r\n\r\n";
            case StatusCode::ServiceUnavailable:
                return "HTTP/1.1 503 Service Unavailable\r\n\r\n";
        }
        return "HTTP/1.1 500 Internal Server Error\r\n\r\n";
    }
    std::string makeResponse(StatusCode code, std::string_view body) {
        // toResponse() already has \r\n\r\n at the end — strip it and rebuild properly
        std::string_view statusLine;
        switch (code) {
            case StatusCode::Ok:    statusLine = "HTTP/1.1 200 OK";    break;
            case StatusCode::NotFound: statusLine = "HTTP/1.1 404 Not Found"; break;
            // ... rest of cases
            default: statusLine = "HTTP/1.1 500 Internal Server Error"; break;
        }
        return std::string(statusLine)
             + "\r\nContent-Type: text/plain"
             + "\r\nContent-Length: " + std::to_string(body.size())
             + "\r\nConnection: close"
             + "\r\n\r\n"
             + std::string(body);
    }
} // namespace Net::Http
int main(){
    auto server = Net::Servers::Tcp{};
    if (auto r = server.init(Net::IPType::IPv4); !r)
        std::println("init failed: {}", Net::toErrorString(r.error()));


    if (auto r = server.setReusePort(); !r)
        std::println("setReusePort failed: {}", Net::toErrorString(r.error()));
    if (auto r = server.setReuseAddress(); !r)
        std::println("setReuseAddress failed: {}", Net::toErrorString(r.error()));


        
    if (auto r = server.bind("0.0.0.0", 8080); !r)
        std::println("bind failed: {}", Net::toErrorString(r.error()));

    if(auto r = server.listen(); !r)
        std::println("listen failed: {}", Net::toErrorString(r.error()));

 

    std::chrono::milliseconds receiveTimeout(2000);
    std::chrono::milliseconds sendTimeout(2000);
    auto client = server.accept();
    if (!client)
        return 0;
    if (auto r = client.value()->setReceiveTimeout(receiveTimeout); !r)
    {
        std::println("setReceiveTimeout failed: {}", Net::toErrorString(r.error()));
    }
    if (auto r = client.value()->setSendTimeout(sendTimeout); !r)
    {
        std::println("setSendTimeout failed: {}", Net::toErrorString(r.error()));
    }
    auto [ip, port] = client.value()->getRemoteIpPort();
    while (true) {


        std::array<uint8_t, 1024> buf{};
        auto received = client.value()->receive(buf.data(), buf.size());
        if (!received){
            if(received.error() == Error::ConnectionTimeout){
                std::println("TimeOut has occured for: {}:{}",ip, port);
                continue;
            }
            std::println("recv error: {}", Net::toErrorString(received.error()));
            break;
        }

        std::println("Rquest: {}", std::string_view(reinterpret_cast<char*>(buf.data()), received.value()));


        auto response  = Http::makeResponse(Net::Http::StatusCode::Ok,"fuck Yasser ^^");
        auto sent = client.value()->send(response.data(), response.size());
        if (!sent){
            if(sent.error() == Error::ConnectionTimeout){
                std::println("sent TimeOut has occured for: {}:{}",ip, port);
                continue;
            }
            std::println("send error: {}", Net::toErrorString(sent.error()));
            continue;
        }
        std::println("Sent response to the user, {} (bytes)",sent.value());
    };
}
