#pragma once
#include "types.hpp"






namespace Net {

    /// Represents a network address (IPv4 or IPv6) with port.
    /// Cannot be constructed directly — use one of the static `from()` factories.
    class Address {
        public:
            Address(const IPType ipType) noexcept;

            /// Constructs an Address from a human-readable IP string and port.
            /// Accepts both IPv4 ("192.168.1.1") and IPv6 ("::1") formats.
            /// Returns an error if the IP string is invalid or port is 0.
            static Result<Address> from(const std::string& ip, uint16_t port) noexcept;

            /// Constructs an Address from a generic sockaddr_storage.
            /// Automatically dispatches to the correct IPv4 or IPv6 overload
            /// based on ss_family. Use this when accepting connections from the OS.
            static Address from(const sockaddr_storage& address) noexcept;

            /// Constructs an Address from a populated sockaddr_in (IPv4).
            /// Preserves the port and address as-is without conversion.
            static Address from(const sockaddr_in& address) noexcept;

            /// Constructs an Address from a populated sockaddr_in6 (IPv6).
            /// Preserves port, address, and scope_id as-is without conversion.
            static Address from(const sockaddr_in6& address) noexcept;

            /// Returns the IP address as a human-readable string.
            /// Returns an error if the address has not been initialized.
            const Result<const std::string> getIp() const noexcept;

            /// Returns the port number in host byte order.
            /// Returns an error if the address has not been initialized.
            Result<uint16_t> getPort() const noexcept;

            /// Returns whether this address is IPv4 or IPv6.
            Result<IPType> getIpType() const noexcept {
                return fromAddressFamily(address_.ss_family);
            }

            /// Returns a pointer to the raw sockaddr struct for use in OS socket calls
            /// such as bind(), connect(), and accept().
            const sockaddr* getAddrRaw() const noexcept { return reinterpret_cast<const sockaddr*>(&address_); }

            // return a mutable  pointer to the raw sockaddr
            sockaddr* getAddrRaw() noexcept { return reinterpret_cast<sockaddr*>(&address_); }




            /// Returns the size of the underlying address struct in bytes.
            /// Needed alongside raw() for OS socket calls.
            socklen_t getSize() const noexcept {
                return size_;
            }
            // Returns a pointer to the address length, allowing recvfrom/accept to write the actual size back
            socklen_t* getSizeRaw() noexcept {
                return &size_;
            }

            Address() = default;


        private:


            socklen_t  size_{sizeof(sockaddr_in6)};      // actual size of the active address struct
            sockaddr_storage address_{};    // storage large enough for IPv4 and IPv6
    };

}
