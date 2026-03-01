#include "../include/address.hpp"

namespace Net {

Address::Address(const IPType ipType) noexcept {
  size_ = getAddressSizeByIpType(ipType).value_or(sizeof(sockaddr_in));
}

Address Address::from(const sockaddr_in &address) noexcept {
  Address addr;
  sockaddr_in *a4 = reinterpret_cast<sockaddr_in *>(&addr.address_);
  a4->sin_family = AF_INET;
  a4->sin_port = address.sin_port;
  a4->sin_addr = address.sin_addr;
  addr.size_ = sizeof(sockaddr_in);

  return addr;
}

Address Address::from(const sockaddr_in6 &address) noexcept {
  Address addr;
  sockaddr_in6 *a6 = reinterpret_cast<sockaddr_in6 *>(&addr.address_);
  a6->sin6_family = AF_INET6;
  a6->sin6_port = address.sin6_port;
  a6->sin6_addr = address.sin6_addr;
  a6->sin6_scope_id = address.sin6_scope_id;
  addr.size_ = sizeof(sockaddr_in6);
  return addr;
}

Address Address::from(const sockaddr_storage &address) noexcept {
  if (address.ss_family == AF_INET) {
    return Address::from(*reinterpret_cast<const sockaddr_in *>(&address));
  } else {
    return Address::from(*reinterpret_cast<const sockaddr_in6 *>(&address));
  }
}

Result<Address> Address::from(const std::string &ip, uint16_t port) noexcept {
  if (port == 0)
    return std::unexpected{Error::InvalidPort};
  Address addr;
  sockaddr_in *a4 = reinterpret_cast<sockaddr_in *>(&addr.address_);
  if (inet_pton(AF_INET, ip.c_str(), &a4->sin_addr) == 1) {
    a4->sin_family = AF_INET;
    a4->sin_port = htons(port);
    addr.size_ = sizeof(sockaddr_in);
    return addr;
  }
  sockaddr_in6 *a6 = reinterpret_cast<sockaddr_in6 *>(&addr.address_);
  if (inet_pton(AF_INET6, ip.c_str(), &a6->sin6_addr) == 1) {
    a6->sin6_family = AF_INET6;
    a6->sin6_port = htons(port);
    addr.size_ = sizeof(sockaddr_in6);
    return addr;
  }
  return std::unexpected{Error::InvalidIP};
}

// const sockaddr* Address::raw() const noexcept {
//     if(address_.ss_family == AF_INET)
//         return reinterpret_cast<const sockaddr*>(&address_);
//     else
//         return reinterpret_cast<const sockaddr_in6*>(&address_);
// }
const Result<const std::string> Address::getIp() const noexcept {
  if (size_ == 0)
    return std::unexpected{Error::InvalidIP};

  static thread_local char buffer[INET6_ADDRSTRLEN];

  const void *src = nullptr;
  int family = address_.ss_family;

  if (family == AF_INET) {
    src = &reinterpret_cast<const sockaddr_in *>(&address_)->sin_addr;
  } else {
    src = &reinterpret_cast<const sockaddr_in6 *>(&address_)->sin6_addr;
  }

  if (inet_ntop(family, src, buffer, sizeof(buffer)) == nullptr)
    return std::unexpected{getError()};

  return buffer;
}

Result<uint16_t> Address::getPort() const noexcept {
  if (size_ == 0)
    return std::unexpected{Error::InvalidPort};

  if (address_.ss_family == AF_INET) {
    return ntohs(reinterpret_cast<const sockaddr_in *>(&address_)->sin_port);
  } else {
    return ntohs(reinterpret_cast<const sockaddr_in6 *>(&address_)->sin6_port);
  }
}

} // namespace Net
