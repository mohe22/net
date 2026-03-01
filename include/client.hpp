#pragma once
#include "types.hpp"
#include "address.hpp"
namespace  Net {
    class Client {
        public:
            Client() = default;
            Client(SocketHandle socket, const Address& address) noexcept;

            Client(const Client&) = delete;
            Client(Client&&) = delete;
            Client& operator=(const Client&) = delete;
            Client& operator=(Client&&) = delete;
            ~Client() noexcept;

            Result<ssize> send(const void* data, size_t size);
            Result<ssize> receive(void* data, size_t size);
            Result<void> close();

        private:
            SocketHandle socket_{invaliedSocket};
            Address address_{};
    };
}
