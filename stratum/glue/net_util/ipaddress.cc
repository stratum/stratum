// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


// Definition of classes IPAddress and SocketAddress.

#include "stratum/glue/net_util/ipaddress.h"

#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>  // for snprintf
#include <sys/socket.h>
#include <iosfwd>
#include <limits>
#include <string>
#include <vector>

#include "absl/numeric/int128.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/strip.h"
#include "absl/strings/substitute.h"
#include "stratum/glue/net_util/bits.h"

// TODO(unknown) not required for Google internally
using absl::numbers_internal::safe_strto32_base;
using absl::numbers_internal::safe_strtou64_base;

namespace stratum {

#define gntohl(x) ntohl(x)
#define ghtonl(x) htonl(x)
#define gntohs(x) ntohs(x)
#define ghtons(x) htons(x)

#define ascii_isxdigit(x) isxdigit(x)

namespace {

const int kMaxNetmaskIPv4 = 32;
const int kMaxNetmaskIPv6 = 128;

}  // anonymous namespace

// Sanity check: be sure INET_ADDRSTRLEN fits into INET6_ADDRSTRLEN.
// ToCharBuf() (below) depends on this.
static_assert(INET_ADDRSTRLEN <= INET6_ADDRSTRLEN, "ipv6 larger than ipv4");

IPAddress IPAddress::Any4() {
  return HostUInt32ToIPAddress(INADDR_ANY);
}

IPAddress IPAddress::Loopback4() {
  return HostUInt32ToIPAddress(INADDR_LOOPBACK);
}

IPAddress IPAddress::Any6() {
  return IPAddress(in6addr_any);
}

IPAddress IPAddress::Loopback6() {
  return IPAddress(in6addr_loopback);
}

IPAddress UInt128ToIPAddress(const absl::uint128& bigint) {
  in6_addr addr6;
  addr6.s6_addr32[0] =
      ghtonl(static_cast<uint32>(absl::Uint128High64(bigint) >> 32));
  addr6.s6_addr32[1] =
      ghtonl(static_cast<uint32>(absl::Uint128High64(bigint) & 0xFFFFFFFFULL));
  addr6.s6_addr32[2] =
      ghtonl(static_cast<uint32>(absl::Uint128Low64(bigint) >> 32));
  addr6.s6_addr32[3] =
      ghtonl(static_cast<uint32>(absl::Uint128Low64(bigint) & 0xFFFFFFFFULL));
  return IPAddress(addr6);
}

bool IsAnyIPAddress(const IPAddress& ip) {
  switch (ip.address_family()) {
    case AF_INET:
      return (ip == IPAddress::Any4());
    case AF_INET6:
      return (ip == IPAddress::Any6());
    case AF_UNSPEC:
      LOG(DFATAL) << "Calling IsAnyIPAddress() on an empty IPAddress";
      break;
    default:
      LOG(DFATAL)
          << "Calling IsAnyIPAddress() on an IPAddress "
          << "with unknown address family "
          << ip.address_family();
  }
  return false;
}

namespace {

enum class LoopbackMode {
  INCLUDE_ENTIRE_IPV4_LOOPBACK_NETWORK,
  DO_NOT_INCLUDE_ENTIRE_IPV4_LOOPBACK_NETWORK,
};

bool IsLoopbackIPAddress(const IPAddress& ip,
                         const LoopbackMode mode) {
  switch (ip.address_family()) {
    case AF_INET:
      if (mode == LoopbackMode::INCLUDE_ENTIRE_IPV4_LOOPBACK_NETWORK) {
        return IsWithinSubnet(IPRange(IPAddress::Loopback4(), 8), ip);
      } else {
        return ip == IPAddress::Loopback4();
      }
    case AF_INET6:
      return (ip == IPAddress::Loopback6());
    case AF_UNSPEC:
      LOG(DFATAL) << "Calling IsLoopbackIPAddress() on an empty IPAddress";
      break;
    default:
      LOG(DFATAL)
          << "Calling IsLoopbackIPAddress() on an IPAddress "
          << "with unknown address family "
          << ip.address_family();
  }
  return false;
}

}  // namespace

bool IsCanonicalLoopbackIPAddress(const IPAddress& ip) {
  return IsLoopbackIPAddress(
      ip, LoopbackMode::DO_NOT_INCLUDE_ENTIRE_IPV4_LOOPBACK_NETWORK);
}

bool IsLoopbackIPAddress(const IPAddress& ip) {
  return IsLoopbackIPAddress(
      ip, LoopbackMode::INCLUDE_ENTIRE_IPV4_LOOPBACK_NETWORK);
}

// <buffer> must have room for at least INET6_ADDRSTRLEN bytes,
// including the final NUL.
void IPAddress::ToCharBuf(char* buffer) const {
  switch (address_family_) {
    case AF_INET: {
      CHECK(inet_ntop(AF_INET, &addr_.addr4, buffer, INET_ADDRSTRLEN)
            != NULL);
      break;
    }
    case AF_INET6:
      CHECK(inet_ntop(AF_INET6, &addr_.addr6, buffer, INET6_ADDRSTRLEN)
            != NULL);
      break;
    case AF_UNSPEC:
      LOG(DFATAL) << "Calling ToCharBuf() on an empty IPAddress";
      *buffer = '\0';
      break;
    default:
      LOG(FATAL) << "Unknown address family " << address_family_;
  }
}

std::string IPAddress::ToString() const {
  char buf[INET6_ADDRSTRLEN];
  ToCharBuf(buf);
  return buf;
}

std::string IPAddress::ToPackedString() const {
  switch (address_family_) {
    case AF_INET:
      return std::string(reinterpret_cast<const char *>(&addr_.addr4),
                         sizeof(addr_.addr4));
    case AF_INET6:
      return std::string(reinterpret_cast<const char *>(&addr_.addr6),
                         sizeof(addr_.addr6));
    case AF_UNSPEC:
      LOG(DFATAL) << "Calling ToPackedString() on an empty IPAddress";
      return "";
    default:
      LOG(FATAL) << "Unknown address family " << address_family_;
  }
}

bool StringToIPAddress(const char* str, IPAddress* out) {
  // Try to parse the string as an IPv4 address first. (glibc does not
  // yet recognize IPv4 addresses when given an address family of
  // AF_INET, but at some point it will, and at that point the other way
  // around would break.)
  in_addr addr4;
  if (str && inet_pton(AF_INET, str, &addr4) > 0) {
    if (out) {
      *out = IPAddress(addr4);
    }
    return true;
  }

  in6_addr addr6;
  if (str && inet_pton(AF_INET6, str, &addr6) > 0) {
    if (out) {
      *out = IPAddress(addr6);
    }
    return true;
  }

  return false;
}

bool PackedStringToIPAddress(const std::string& str, IPAddress* out) {
  if (str.length() == sizeof(in_addr)) {
    if (out) {
      in_addr addr;
      memcpy(&addr, str.data(), sizeof(addr));
      *out = IPAddress(addr);
    }
    return true;
  } else if (str.length() == sizeof(in6_addr)) {
    if (out) {
      in6_addr addr;
      memcpy(&addr, str.data(), sizeof(addr));
      *out = IPAddress(addr);
    }
    return true;
  }

  return false;
}

namespace {

// Parse a 64-bit number, using exactly 16 hex digits.
bool InternalParseHexUInt64(const std::string& hex_str, uint64* out) {
  const size_t kExpectedLength = 16;
  if (hex_str.length() != kExpectedLength) {
    return false;
  }
  for (size_t ix = 0; ix < kExpectedLength; ix++) {
    if (!ascii_isxdigit(hex_str[ix])) {
      return false;
    }
  }
  return safe_strtou64_base(hex_str, out, 16);
}

}  // namespace

bool ColonlessHexToIPv6Address(const std::string& hex_str, IPAddress* ip6) {
  // Parse a hex string like "fe80000000000000000573fffea00065" into a
  // valid IPv6 address, if possible.
  uint64 hi = 0;
  uint64 lo = 0;
  if (InternalParseHexUInt64(hex_str.substr(0, 16), &hi) &&
      InternalParseHexUInt64(hex_str.substr(16), &lo)) {
    if (ip6) {
      *ip6 = UInt128ToIPAddress(absl::MakeUint128(hi, lo));
    }
    return true;
  }
  return false;
}

bool IPAddress::operator==(const IPAddress& other) const {
  if (address_family_ != other.address_family_) {
    return false;
  }

  switch (address_family_) {
    case AF_INET:
      return memcmp(&addr_.addr4, &other.addr_.addr4, sizeof(addr_.addr4)) == 0;
    case AF_INET6:
      return memcmp(&addr_.addr6, &other.addr_.addr6, sizeof(addr_.addr6)) == 0;
    case AF_UNSPEC:
      // We've already verified that they've got the same address family.
      return true;
    default:
      LOG(FATAL) << "Unknown address family " << address_family_;
  }
}

std::string IPAddressToURIString(const IPAddress& ip) {
  switch (ip.address_family()) {
    case AF_INET6:
      return absl::Substitute("[$0]", ip.ToString().c_str());
    default:
      return ip.ToString();
  }
}

std::string IPAddressToPTRString(const IPAddress& ip) {
  char ptr_name[sizeof("0.1.2.3.4.5.6.7.8.9.a.b.c.d.e.f."
                       "0.1.2.3.4.5.6.7.8.9.a.b.c.d.e.f.ip6.arpa")];
  memset(ptr_name, 0, sizeof(ptr_name));

  switch (ip.address_family()) {
    case AF_INET: {
      int a1, a2, a3, a4;
      const uint32 addr = IPAddressToHostUInt32(ip);
      a1 = static_cast<int>((addr >> 24) & 0xff);
      a2 = static_cast<int>((addr >> 16) & 0xff);
      a3 = static_cast<int>((addr >> 8) & 0xff);
      a4 = static_cast<int>(addr & 0xff);
      snprintf(ptr_name, sizeof(ptr_name), "%d.%d.%d.%d.in-addr.arpa",
               a4, a3, a2, a1);
      return ptr_name;
    }
    case AF_INET6: {
      const struct in6_addr addr = ip.ipv6_address();
      const unsigned char * bytes =
          reinterpret_cast<const unsigned char *>(addr.s6_addr);
      snprintf(ptr_name, sizeof(ptr_name),
               "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
               "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.ip6.arpa",
               bytes[15] & 0xf, bytes[15] >> 4,
               bytes[14] & 0xf, bytes[14] >> 4,
               bytes[13] & 0xf, bytes[13] >> 4,
               bytes[12] & 0xf, bytes[12] >> 4,
               bytes[11] & 0xf, bytes[11] >> 4,
               bytes[10] & 0xf, bytes[10] >> 4,
               bytes[9] & 0xf, bytes[9] >> 4,
               bytes[8] & 0xf, bytes[8] >> 4,
               bytes[7] & 0xf, bytes[7] >> 4,
               bytes[6] & 0xf, bytes[6] >> 4,
               bytes[5] & 0xf, bytes[5] >> 4,
               bytes[4] & 0xf, bytes[4] >> 4,
               bytes[3] & 0xf, bytes[3] >> 4,
               bytes[2] & 0xf, bytes[2] >> 4,
               bytes[1] & 0xf, bytes[1] >> 4,
               bytes[0] & 0xf, bytes[0] >> 4);
      return ptr_name;
    }
    case AF_UNSPEC:
      LOG(DFATAL) << "Calling IPAddressToPTRString() on an empty IPAddress";
      return "unspecified.arpa";
    default:
      LOG(FATAL) << "Unknown address family " << ip.address_family();
  }
}

namespace {

const int kIP6HexDigits = 32;
const int kIP6WithDotsLength = (kIP6HexDigits * 2) - 1;
const char kIPv4Suffix[] = ".in-addr.arpa";
const char kIPv6Suffix[] = ".ip6.arpa";

bool InternalParseIPv4PTRString(const char* host, IPAddress* out) {
  // Treat the input as an IPv4 address with reversed octets.
  in_addr addr;
  if (inet_pton(AF_INET, host, &addr) > 0) {
    addr.s_addr = bswap_32(addr.s_addr);
    if (out) {
      *out = IPAddress(addr);
    }
    return true;
  }
  return false;
}

bool InternalParseIPv6PTRString(const std::string& host, IPAddress* out) {
  if (host.length() != static_cast<size_t>(kIP6WithDotsLength)) {
    return false;
  }
  std::string reversed_without_dots;
  reversed_without_dots.reserve(kIP6HexDigits);
  for (int i = kIP6WithDotsLength; i > 0; i -= 2) {
    DCHECK_EQ(i % 2, 1);
    if (i != kIP6WithDotsLength && host[i] != '.') {
      return false;
    }
    reversed_without_dots.push_back(host[i - 1]);
  }
  return ColonlessHexToIPv6Address(reversed_without_dots, out);
}

}  // anonymous namespace

bool PTRStringToIPAddress(const std::string& ptr_string, IPAddress* out) {
  std::string host = std::string(absl::StripSuffix(ptr_string, "."));
  if (absl::EndsWith(host, kIPv4Suffix)) {
    host = std::string(absl::StripSuffix(host, kIPv4Suffix));
    return InternalParseIPv4PTRString(host.c_str(), out);
  } else if (absl::EndsWith(host, kIPv6Suffix)) {
    host = std::string(absl::StripSuffix(host, kIPv6Suffix));
    return InternalParseIPv6PTRString(host, out);
  } else {
    return false;
  }
}

// Return a random IP from the choices in hp. Assumes that hp->h_addrtype is
// AF_INET or AF_INET6. (Adapted from net/base/dnscache.cc.)
IPAddress ChooseRandomAddress(const struct hostent* hp) {
  LOG(FATAL) << __FUNCTION__ << " not yet implemented in depot3";
}

// Return a random IPAddress from the a vector of same.
IPAddress ChooseRandomIPAddress(const std::vector<IPAddress> *ipvec) {
  LOG(FATAL) << __FUNCTION__ << " not yet implemented in depot3";
}

bool IPAddressOrdering::operator()(const IPAddress& lhs,
                                   const IPAddress& rhs) const {
  if (lhs.address_family_ != rhs.address_family_) {
    return lhs.address_family_ < rhs.address_family_;
  }

  switch (lhs.address_family_) {
    case AF_INET: {
      return IPAddressToHostUInt32(lhs) < IPAddressToHostUInt32(rhs);
    }
    case AF_INET6: {
      const uint32* lhs_addr6 = lhs.addr_.addr6.s6_addr32;
      const uint32* rhs_addr6 = rhs.addr_.addr6.s6_addr32;

      if (lhs_addr6[0] != rhs_addr6[0])
        return gntohl(lhs_addr6[0]) < gntohl(rhs_addr6[0]);
      if (lhs_addr6[1] != rhs_addr6[1])
        return gntohl(lhs_addr6[1]) < gntohl(rhs_addr6[1]);
      if (lhs_addr6[2] != rhs_addr6[2])
        return gntohl(lhs_addr6[2]) < gntohl(rhs_addr6[2]);
      return gntohl(lhs_addr6[3]) < gntohl(rhs_addr6[3]);
    }
    case AF_UNSPEC: {
      // Unspecified address families are considered equal.
      return false;
    }
    default: {
      LOG(FATAL) << "Unknown address family " << lhs.address_family_;
    }
  }
}

bool SocketAddressOrdering::operator()(const SocketAddress& lhs,
                                       const SocketAddress& rhs) const {
  if (!IsInitializedSocketAddress(rhs)) {
    return false;
  }
  if (!IsInitializedSocketAddress(lhs)) {
    return true;
  }
  IPAddressOrdering ip_address_ordering;
  if (lhs.host() != rhs.host()) {
    return ip_address_ordering(lhs.host(), rhs.host());
  }
  return lhs.port() < rhs.port();
}

bool IPRangeOrdering::operator()(const IPRange& lhs,
                                 const IPRange& rhs) const {
  if (!IsInitializedRange(rhs)) {
    return false;
  }
  if (!IsInitializedRange(lhs)) {
    return true;
  }
  IPAddressOrdering ip_address_ordering;
#if 0
  if (lhs.network_address() != rhs.network_address()) {
    return ip_address_ordering(lhs.network_address(), rhs.network_address());
  }
#else
  // host() and network_address() are the same on truncated IPRanges
  // truncated IPRanges are now the default.  Use host() for
  // performance assuming that no-one manually constructs an unsafe,
  // untruncated IPRange.
  if (lhs.host() != rhs.host()) {
    return ip_address_ordering(lhs.host(), rhs.host());
  }
#endif
  return lhs.length() < rhs.length();
}

bool GetCompatIPv4Address(const IPAddress& ip6, IPAddress* ip4) {
  if (ip6.address_family() != AF_INET6) {
    DCHECK_NE(AF_UNSPEC, ip6.address_family());
    return false;
  }

  in6_addr addr6 = ip6.ipv6_address();
  if (addr6.s6_addr32[0] != 0 || addr6.s6_addr32[1] != 0 ||
      addr6.s6_addr32[2] != 0) {
    return false;
  }

  // :: and ::1 are special cases and should not be treated as compatible
  // addresses; see http://en.wikipedia.org/wiki/IPv4-compatible_address.
  if (gntohl(addr6.s6_addr32[3]) == 0 || gntohl(addr6.s6_addr32[3]) == 1) {
    return false;
  }

  if (ip4) {
    in_addr ipv4;
    ipv4.s_addr = addr6.s6_addr32[3];
    *ip4 = IPAddress(ipv4);
  }

  return true;
}

bool GetMappedIPv4Address(const IPAddress& ip6, IPAddress* ip4) {
  if (ip6.address_family() != AF_INET6) {
    DCHECK_NE(AF_UNSPEC, ip6.address_family());
    return false;
  }

  in6_addr addr6 = ip6.ipv6_address();
  if (addr6.s6_addr32[0] != 0 || addr6.s6_addr32[1] != 0 ||
      addr6.s6_addr16[4] != 0 || addr6.s6_addr16[5] != 0xffff) {
    return false;
  }

  if (ip4) {
    in_addr ipv4;
    ipv4.s_addr = addr6.s6_addr32[3];
    *ip4 = IPAddress(ipv4);
  }

  return true;
}

bool Get6to4IPv4Address(const IPAddress& ip6, IPAddress* ip4) {
  if (ip6.address_family() != AF_INET6) {
    DCHECK_NE(AF_UNSPEC, ip6.address_family());
    return false;
  }

  in6_addr addr6 = ip6.ipv6_address();
  if (addr6.s6_addr16[0] != ghtons(0x2002)) {
    return false;
  }

  if (ip4) {
    in_addr addr4;
    DCHECK_EQ(4u, sizeof(addr4));
    memcpy(&addr4, &addr6.s6_addr16[1], sizeof(addr4));
    *ip4 = IPAddress(addr4);
  }

  return true;
}

bool Get6to4IPv6Range(const IPRange& iprange4, IPRange* iprange6) {
  if (iprange4.host().address_family() != AF_INET) {
    DCHECK_NE(AF_UNSPEC, iprange4.host().address_family());
    return false;
  }

  if (iprange6) {
    in6_addr addr6;
    in_addr addr4 = iprange4.host().ipv4_address();

    memset(&addr6, 0, sizeof(addr6));
    addr6.s6_addr16[0] = ghtons(0x2002);
    DCHECK_EQ(4u, sizeof(addr4));
    memcpy(&addr6.s6_addr16[1], &addr4, sizeof(addr4));

    *iprange6 = IPRange::UnsafeConstruct(
        IPAddress(addr6), iprange4.length() + 16);
  }
  return true;
}

bool GetIsatapIPv4Address(const IPAddress& ip6, IPAddress* ip4) {
  if (ip6.address_family() != AF_INET6) {
    DCHECK_NE(AF_UNSPEC, ip6.address_family());
    return false;
  }

  // If it's a Teredo address with the right port (41217, or 0xa101) which
  // would be encoded as 0x5efe then it can't be an ISATAP address.
  if (GetTeredoInfo(ip6, NULL, NULL, NULL, NULL)) {
    return false;
  }

  in6_addr addr6 = ip6.ipv6_address();
  // ISATAP addresses are identifiable by the 32bit 0000:5efe
  // prepended to the client's IPv4 address to form the 64bit
  // interface identifier.  The usual rules about U/L and G bits
  // apply as well, hence we mask those bits when testing for equality.
  if (addr6.s6_addr16[5] != ghtons(0x5efe)
      || (addr6.s6_addr16[4] | ghtons(0x0300)) !=  ghtons(0x0300)) {
    return false;
  }

  if (ip4) {
    in_addr ipv4;
    ipv4.s_addr = addr6.s6_addr32[3];
    *ip4 = IPAddress(ipv4);
  }

  return true;
}

bool GetTeredoInfo(const IPAddress& ip6, IPAddress* server, uint16* flags,
                   uint16* port, IPAddress* client) {
  if (ip6.address_family() != AF_INET6) {
    DCHECK_NE(AF_UNSPEC, ip6.address_family());
    return false;
  }

  in6_addr addr6 = ip6.ipv6_address();
  if (addr6.s6_addr16[0] != ghtons(0x2001) || addr6.s6_addr16[1] != 0)
    return false;

  in_addr ipv4;
  if (client) {
    ipv4.s_addr = ~(addr6.s6_addr32[3]);
    *client = IPAddress(ipv4);
  }
  if (server) {
    ipv4.s_addr = addr6.s6_addr32[1];
    *server = IPAddress(ipv4);
  }

  if (port) {
    *port = gntohs(~addr6.s6_addr16[5]);
  }
  if (flags) {
    *flags = gntohs(addr6.s6_addr16[4]);
  }

  return true;
}

bool GetEmbeddedIPv4ClientAddress(const IPAddress& ip6, IPAddress *ip4) {
  // Return the IPv4 Compat, IPv4 Mapped, 6to4, or Teredo client address,
  // if applicable.  NOTE: ISATAP addresses are explicityly NOT returned:
  // the client addresses are not part of the routing information and
  // are, consequently, considerably more spoofable.
  return (GetCompatIPv4Address(ip6, ip4) ||
          GetMappedIPv4Address(ip6, ip4) ||
          Get6to4IPv4Address(ip6, ip4)   ||
          GetTeredoInfo(ip6, NULL, NULL, NULL, ip4));
}

IPAddress NormalizeIPAddress(const IPAddress& ip) {
  if (ip.address_family() != AF_INET6) {
    return ip;
  }

  IPAddress normalized_ip;
  if (GetMappedIPv4Address(ip, &normalized_ip)) {
    return normalized_ip;
  }

  // Not an IPv4 address stored in an IPv6 address; just return it
  // unchanged.
  return ip;
}

IPAddress DualstackIPAddress(const IPAddress& ip) {
  if (ip.address_family() == AF_INET6) {
    return ip;
  }

  CHECK_EQ(AF_INET, ip.address_family());
  struct in6_addr v4mapped;
  v4mapped.s6_addr32[0] = 0;
  v4mapped.s6_addr32[1] = 0;
  v4mapped.s6_addr32[2] = htonl(0xffff);
  v4mapped.s6_addr32[3] = ip.ipv4_address().s_addr;
  DCHECK(IN6_IS_ADDR_V4MAPPED(&v4mapped))
      << "Conversion of " << ip << " to a dualstack IP address failed.";

  return IPAddress(v4mapped);
}

SocketAddress::SocketAddress(const struct sockaddr& saddr) {
  switch (saddr.sa_family) {
    case AF_INET: {
      const struct sockaddr_in* sin
          = reinterpret_cast<const struct sockaddr_in*>(&saddr);
      CHECK_EQ(AF_INET, sin->sin_family);
      host_ = IPAddress(sin->sin_addr);
      port_ = ntohs(sin->sin_port);
      break;
    }
    case AF_INET6: {
      const struct sockaddr_in6* sin6
          = reinterpret_cast<const struct sockaddr_in6*>(&saddr);
      CHECK_EQ(AF_INET6, sin6->sin6_family);
      host_ = IPAddress(sin6->sin6_addr);
      port_ = ntohs(sin6->sin6_port);
      break;
    }
    case AF_UNSPEC: {
      host_ = IPAddress();
      port_ = 0;
      break;
    }
    default: {
      LOG(FATAL) << "Unknown address family " << saddr.sa_family;
    }
  }
}

// Essentially the same as the one above (ie. switch to a delegating
// constructor once we go to C++0x).
SocketAddress::SocketAddress(const struct sockaddr_storage& saddr) {
  switch (saddr.ss_family) {
    case AF_INET: {
      const struct sockaddr_in* sin
          = reinterpret_cast<const struct sockaddr_in*>(&saddr);
      CHECK_EQ(AF_INET, sin->sin_family);
      host_ = IPAddress(sin->sin_addr);
      port_ = ntohs(sin->sin_port);
      break;
    }
    case AF_INET6: {
      const struct sockaddr_in6* sin6
          = reinterpret_cast<const struct sockaddr_in6*>(&saddr);
      CHECK_EQ(AF_INET6, sin6->sin6_family);
      host_ = IPAddress(sin6->sin6_addr);
      port_ = ntohs(sin6->sin6_port);
      break;
    }
    case AF_UNSPEC: {
      host_ = IPAddress();
      port_ = 0;
      break;
    }
    default: {
      LOG(FATAL) << "Unknown address family " << saddr.ss_family;
    }
  }
}

std::string SocketAddress::ToString() const {
  if (!IsInitializedAddress(host_)) {
    LOG(DFATAL) << "Calling ToString() on an empty SocketAddress";
    return "";
  }
  return absl::Substitute("$0:$1", IPAddressToURIString(host_).c_str(), port_);
}

std::string SocketAddress::ToPackedString() const {
  LOG(FATAL) << __FUNCTION__ << " not yet implemented in depot3";
  return "";
}

IPAddress IPRange::network_address() const {
  switch (host_.address_family()) {
    case AF_INET:
    case AF_INET6:
      DCHECK_EQ(host_, TruncateIPAddress(host_, length_));
      return host_;
    default:
      LOG(FATAL) << "Unknown address family " << host_.address_family();
  }
}

IPAddress IPRange::broadcast_address() const {
  switch (host_.address_family()) {
    case AF_INET: {
      if (length_ == 0) {
        return HostUInt32ToIPAddress(~0U);
      }

      // OR the address with a mask of "length_" leading zeroes and the
      // remainder of the bits set to one.
      const uint32 addr32 = IPAddressToHostUInt32(host_);
      return HostUInt32ToIPAddress(addr32 | (~(~0U << (32 - length_))));
    }
    case AF_INET6: {
      if (length_ == 0) {
        return UInt128ToIPAddress(~absl::uint128(0));
      }

      // OR the address with a mask of "length_" leading zeroes and the
      // remainder of the bits set to one.
      const absl::uint128 addr128 = IPAddressToUInt128(host_);
      return UInt128ToIPAddress(addr128 |
                                ~(~absl::uint128(0) << (128 - length_)));
    }
    default: {
      LOG(FATAL) << "Unknown address family " << host_.address_family();
    }
  }
}

bool StringToSocketAddress(const std::string& str, SocketAddress* out) {
  LOG(FATAL) << __FUNCTION__ << " not yet implemented in depot3";
}

bool StringToSocketAddressWithDefaultPort(const std::string& str,
                                          uint16 default_port,
                                          SocketAddress* out) {
  LOG(FATAL) << __FUNCTION__ << " not yet implemented in depot3";
}

sockaddr_storage SocketAddress::generic_address() const {
  sockaddr_storage ret;
  socklen_t size;
  CHECK(SocketAddressToFamily(AF_UNSPEC, *this, &ret, &size))
      << "Called generic_address() on " << *this;
  return ret;
}

bool SocketAddressToFamily(int output_family, const SocketAddress& sa,
                           sockaddr_storage* addr_out, socklen_t* size_out) {
  const IPAddress host = sa.host();
  if (output_family == AF_UNSPEC) {
    output_family = host.address_family();
  }
  switch (output_family) {
    case AF_INET: {
      sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(addr_out);
      memset(addr, 0, sizeof(*addr));
      if (size_out != NULL) {
        *size_out = sizeof(*addr);
      }
      if (host.address_family() == AF_INET) {
        addr->sin_family = AF_INET;
        addr->sin_addr = host.ipv4_address();
        addr->sin_port = htons(sa.port());
        return true;
      } else if (host == IPAddress::Any6()) {
        // Binding to :: can be useful regardless of the socket family.
        addr->sin_family = AF_INET;
        DCHECK_EQ(IPAddress(addr->sin_addr), IPAddress::Any4());
        addr->sin_port = htons(sa.port());
        return true;
      }
      break;
    }
    case AF_INET6: {
      sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(addr_out);
      memset(addr, 0, sizeof(*addr));
      if (size_out != NULL) {
        *size_out = sizeof(*addr);
      }
      if (host.address_family() == AF_INET6) {
        addr->sin6_family = AF_INET6;
        addr->sin6_addr = host.ipv6_address();
        addr->sin6_port = htons(sa.port());
        return true;
      } else if (host.address_family() == AF_INET) {
        // Convert IPv4 to IPv6, for use in dualstack sockets.
        addr->sin6_family = AF_INET6;
        addr->sin6_addr = DualstackIPAddress(host).ipv6_address();
        addr->sin6_port = htons(sa.port());
        return true;
      }
      break;
    }
  }
  // Generate an invalid sockaddr, to prevent accidental use.
  LOG(WARNING) << "Can't convert address family "
               << host.address_family() << " to " << output_family;
  memset(addr_out, 0, sizeof(sockaddr));
  addr_out->ss_family = 0xFFFF;
  if (size_out != NULL) {
    *size_out = 0;
  }
  return false;
}

bool SocketAddressToFamilyForBind(int output_family,
                                  const SocketAddress& sa,
                                  sockaddr_storage* addr_out,
                                  socklen_t* size_out) {
  SocketAddress sa_copy(sa);
  if (output_family == AF_INET6 && sa.host() == IPAddress::Any4()) {
    // Convert 0.0.0.0:port to [::]:port.
    sa_copy = SocketAddress(IPAddress::Any6(), sa.port());
  }
  return SocketAddressToFamily(output_family, sa_copy, addr_out, size_out);
}

namespace {

bool InternalStringToNetmaskLength(const char* str, int host_address_family,
                                   int* length) {
  // strto32() will accept a + or - as valid, so explicitly check that the
  // first character is a digit before going on.
  if (!str || !(str[0] >= '0' && str[0] <= '9')) {
    return false;
  }

  int32 parsed_length = 0;
  if (!safe_strto32_base(str, &parsed_length, 10)) {
    // Malformed or missing prefix length, or junk after it.
    // Continue on to the next test...
    parsed_length = -1;
  }

  if (parsed_length < 0 && host_address_family == AF_INET) {
    // Check for a netmask in dotted quad form, e.g. "255.255.0.0".
    struct in_addr mask;
    if (inet_pton(AF_INET, str, &mask) > 0) {
      if (mask.s_addr == 0) {
        parsed_length = 0;
      } else {
        // Now we check to make sure we have a sane netmask.
        // The inverted mask in native byte order (+1) will have to be a
        // power of two, if it's valid.
        uint32 inv_mask = (~gntohl(mask.s_addr)) + 1;
        // Power of two iff x & (x - 1) == 0.
        if ((inv_mask & (inv_mask - 1)) == 0) {
          parsed_length = 32 - ffs(gntohl(mask.s_addr)) + 1;
        }
      }
    }
  }

  if (parsed_length < 0
          || parsed_length > kMaxNetmaskIPv6
          || (host_address_family != AF_INET6 &&
              parsed_length > kMaxNetmaskIPv4)) {
    return false;
  }

  if (length) {
    *length = parsed_length;
  }
  return true;
}

//
// The "meat" of StringToIPRange{,AndTruncate}. Does no checking of correct
// prefix length, nor any automatic truncation.
//
bool InternalStringToIPRange(const std::string& str,
                             std::pair<IPAddress, int>* out) {
  DCHECK(out);
  std::string address_string;

  const size_t slash_pos = str.find('/');
  if (slash_pos == std::string::npos) {
    // Missing slash, may be just an IP address string.
    address_string = str;
  } else {
    address_string.assign(str.data(), slash_pos);
  }

  // Try to parse everything before the slash as an IP address.
  if (!StringToIPAddress(address_string, &out->first)) {
    return false;
  }

  // Try to parse everything after the slash as a prefix length.
  if (slash_pos != std::string::npos) {
    const std::string suffix(str.substr(slash_pos + 1));
    return InternalStringToNetmaskLength(suffix.c_str(),
                                         out->first.address_family(),
                                         &out->second);
  }

  // There was no slash, so the range covers a single address.
  out->second = IPAddressLength(out->first);
  return true;
}

}  // namespace

bool StringToIPRange(const std::string& str, IPRange* out) {
  std::pair<IPAddress, int> parsed;
  if (!InternalStringToIPRange(str, &parsed)) {
    return false;
  }
  const IPRange result(parsed.first, parsed.second);
  if (result.host() != parsed.first) {
    // Some bits were truncated.
    return false;
  }
  if (out) {
    *out = result;
  }
  return true;
}

bool StringToIPRangeAndTruncate(const std::string& str, IPRange* out) {
  std::pair<IPAddress, int> parsed;
  if (!InternalStringToIPRange(str, &parsed)) {
    return false;
  }
  if (out) {
    *out = IPRange(parsed.first, parsed.second);
  }
  return true;
}

namespace net_util_internal {

IPAddress TruncateIPAndLength(const IPAddress& addr, int* length_io) {
  const int length = *length_io;
  switch (addr.address_family()) {
    case AF_INET: {
      if (length >= kMaxNetmaskIPv4) {
        *length_io = kMaxNetmaskIPv4;
        return addr;
      }
      CHECK_GE(length, 0);
      if (length == 0)
        return IPAddress::Any4();
      uint32 ip4 = IPAddressToHostUInt32(addr);
      ip4 &= ~0U << (32 - length);
      return HostUInt32ToIPAddress(ip4);
    }
    case AF_INET6: {
      if (length >= kMaxNetmaskIPv6) {
        *length_io = kMaxNetmaskIPv6;
        return addr;
      }
      CHECK_GE(length, 0);
      if (length == 0)
        return IPAddress::Any6();
      absl::uint128 ip6 = IPAddressToUInt128(addr);
      ip6 &= ~absl::uint128(0) << (128 - length);
      return UInt128ToIPAddress(ip6);
    }
    case AF_UNSPEC:
      *length_io = -1;
      return addr;
    default:
      LOG(FATAL) << "Unknown address family " << addr.address_family();
  }
}

}  // namespace net_util_internal

std::string IPRange::ToString() const {
  return absl::Substitute("$0/$1", host_.ToString().c_str(), length_);
}

namespace {
// Constant needed to differentiate between IPv4 range and IPv6 range in
// IPRange::ToPackedString(). The prefix length and address family are stored
// in one byte, with values [0..128] assigned to IPv6 and [200..232] assigned
// to IPv4.
const uint8 kPackedIPRangeIPv4LengthOffset = 200;
}  // namespace

std::string IPRange::ToPackedString() const {
  CHECK(host_.address_family() == AF_INET ||
        host_.address_family() == AF_INET6)
      << "Uninitialized address in IPRange.";
  // Get the host part, with unwanted suffix bits zeroed.
  const std::string packed_host = host_.ToPackedString();
  // Retain only the portion of the string that is within the mask.
  uint8 packed_host_len = (length_ + 7) / 8;
  // Further compress by removing trailing 0s.
  while (packed_host_len > 0 && packed_host.at(packed_host_len - 1) == 0) {
    --packed_host_len;
  }
  // Encode the address family and prefix length into a 1-byte header.
  uint8 header = length_;
  if (host().address_family() == AF_INET) {
    header += kPackedIPRangeIPv4LengthOffset;
  }
  // Put it all together.
  std::string out;
  out.reserve(1 + packed_host_len);
  out.push_back(static_cast<char>(header));
  out.append(packed_host.data(), packed_host_len);
  return out;
}

bool PackedStringToIPRange(const std::string& str, IPRange *out) {
  if (str.empty()) {
    return false;
  }
  const uint8 header = static_cast<uint8>(str[0]);
  const size_t available_host_bytes = str.size() - 1;
  // If we have a packed IPv6 IPRange, then the header will represent the mask
  // length. If it is IPv4 range, than the mask length is obtained from header
  // by substracting kPackedIPRangeIPv4LengthOffset.
  int prefix_len;
  size_t sizeof_addr;
  if (0 <= header && header <= kMaxNetmaskIPv6) {
    prefix_len = header;
    sizeof_addr = sizeof(in6_addr);
  } else if (kPackedIPRangeIPv4LengthOffset <= header &&
             header <= kMaxNetmaskIPv4 + kPackedIPRangeIPv4LengthOffset) {
    prefix_len = header - kPackedIPRangeIPv4LengthOffset;
    sizeof_addr = sizeof(in_addr);
  } else {
    LOG(ERROR) << "Invalid netmask " << static_cast<int>(header)
               << " passed to PackedStringToIPRange. Valid ranges are: 0-"
               << kMaxNetmaskIPv6 << " and "
               << static_cast<int>(kPackedIPRangeIPv4LengthOffset) << "-"
               << kPackedIPRangeIPv4LengthOffset + kMaxNetmaskIPv4 << ".";
    return false;
  }

  // Verify that the input doesn't overflow the address width.
  if (available_host_bytes > sizeof_addr) {
    return false;
  }

  // Drop the address into a zero-padded buffer, and convert to IPAddress.
  std::string packed_host(sizeof_addr, '\0');
  packed_host.replace(0, available_host_bytes,
                      str.data() + 1, available_host_bytes);
  const IPAddress host = PackedStringToIPAddressOrDie(packed_host);

  // Verify that the input has no bits set beyond the prefix length.
  const IPRange truncated(host, prefix_len);
  if (truncated.host() != host) {
    return false;
  }
  if (out) {
    *out = truncated;
  }
  return true;
}

bool IPAddressIntervalToSubnets(const IPAddress& first_addr,
                                const IPAddress& last_addr,
                                std::vector<IPRange>* covering_subnets) {
  covering_subnets->clear();

  // Fail if parameters do not belong to the same valid address family.
  if (first_addr.address_family() != last_addr.address_family() ||
      first_addr.address_family() == AF_UNSPEC) {
    return false;
  }

  IPAddressOrdering less;
  for (IPAddress cur_addr = first_addr; !less(last_addr, cur_addr); ) {
    // Find the least specific IP subnet of cur_addr whose endpoints are still
    // covered by the interval [cur_addr, last_addr].
    IPRange cur_subnet(cur_addr);
    for (int len = IPAddressLength(cur_addr) - 1; len >= 0; --len) {
      const IPRange candidate_subnet(cur_addr, len);
      if (candidate_subnet.host() != cur_addr ||
          less(last_addr, candidate_subnet.broadcast_address())) {
        break;
      }
      cur_subnet = candidate_subnet;
    }

    covering_subnets->push_back(cur_subnet);

    // Find the first address not yet covered by covering_subnets.
    // As a special case, if we covered the max address (e.g. 255.255.255.255),
    // IPAddressPlusN returns false and we are done.
    const IPAddress last_covered_addr = cur_subnet.broadcast_address();
    if (!IPAddressPlusN(last_covered_addr, 1, &cur_addr)) {
      break;
    }
  }

  return !covering_subnets->empty();
}

bool IsRangeIndexValid(const IPRange& range, absl::uint128 index) {
  // Check for potential uint128 >> 128, which is undefined.
  return IPAddressLength(range.host()) - range.length() == 128 ||
         (index >> (IPAddressLength(range.host()) - range.length())) == 0;
}

IPAddress NthAddressInRange(const IPRange& range, absl::uint128 index) {
  CHECK(IsRangeIndexValid(range, index));
  switch (range.host().address_family()) {
    case AF_INET: {
      const uint32 addr = IPAddressToHostUInt32(range.host());
      return HostUInt32ToIPAddress(addr + absl::Uint128Low64(index));
    }
    case AF_INET6: {
      const absl::uint128 addr = IPAddressToUInt128(range.host());
      return UInt128ToIPAddress(addr + index);
    }
    default:
      LOG(FATAL)
          << __FUNCTION__ << " of IPRange with invalid address family: "
          << range.host().address_family();
  }
}

absl::uint128 IndexInRange(const IPRange& range, const IPAddress& ip) {
  CHECK(IsWithinSubnet(range, ip)) << ip << " is not within " << range;
  switch (range.host().address_family()) {
    case AF_INET: {
      const uint32 addr = IPAddressToHostUInt32(range.host());
      return IPAddressToHostUInt32(ip) - addr;
    }
    case AF_INET6: {
      const absl::uint128 addr = IPAddressToUInt128(range.host());
      return IPAddressToUInt128(ip) - addr;
    }
    default:
      LOG(FATAL) << "IPRange with invalid address family: "
                 << range.host().address_family();
  }
}

bool IPAddressPlusN(const IPAddress& addr, int n, IPAddress* result) {
  if (n == 0) {
    *result = addr;
    return true;
  }
  IPAddress addr_copy = addr;
  switch (addr.address_family()) {
    case AF_INET: {
      *result = HostUInt32ToIPAddress(IPAddressToHostUInt32(addr) + n);
      break;
    }
    case AF_INET6:
      *result = UInt128ToIPAddress(IPAddressToUInt128(addr) + n);
      break;
    default:
      LOG(FATAL) << "Invalid address family " << addr.address_family();
  }
  // Return false iff the result crosses the IP address space.
  return (n > 0) == IPAddressOrdering()(addr_copy, *result);
}

bool SubtractIPRange(const IPRange& range, const IPRange& sub_range,
                     std::vector<IPRange>* diff_range) {
  diff_range->clear();

  // Subtract is undefined if "sub_range" is not a more specific of "range".
  if (!IsProperSubRange(range, sub_range)) {
    return false;
  }
  DCHECK_GE(sub_range.length(), 1);

  // An illustrative example using 8-bit IP addressing:
  //   range:      b7  b6  b5  b4  --  --  --  --  /4
  //   sub_range:  b7  b6  b5  b4  b3  b2  b1  b0  /8
  //
  //   diff_range: b7  b6  b5  b4  b3  b2  b1 ~b0  /8
  //               b7  b6  b5  b4  b3  b2 ~b1  --  /7
  //               b7  b6  b5  b4  b3 ~b2  --  --  /6
  //               b7  b6  b5  b4 ~b3  --  --  --  /5
  //
  int address_family = sub_range.host().address_family();
  switch (address_family) {
    case AF_INET: {
      in_addr addr4 = sub_range.network_address().ipv4_address();
      uint32 flip_mask = 1U << (32 - sub_range.length());
      uint32 subnet_mask = ~1U << (32 - sub_range.length());
      for (int len = sub_range.length(); len > range.length(); --len) {
        addr4.s_addr ^= ghtonl(flip_mask);
        diff_range->push_back(IPRange::UnsafeConstruct(IPAddress(addr4), len));
        addr4.s_addr &= ghtonl(subnet_mask);
        flip_mask <<= 1;
        subnet_mask <<= 1;
      }
      break;
    }
    case AF_INET6: {
      absl::uint128 addr128 = IPAddressToUInt128(sub_range.network_address());
      absl::uint128 flip_mask = absl::uint128(1) << (128 - sub_range.length());
      absl::uint128 subnet_mask = ~absl::uint128(1)
                                  << (128 - sub_range.length());
      for (int len = sub_range.length(); len > range.length(); --len) {
        addr128 ^= flip_mask;
        diff_range->push_back(
            IPRange::UnsafeConstruct(UInt128ToIPAddress(addr128), len));
        addr128 &= subnet_mask;
        flip_mask <<= 1;
        subnet_mask <<= 1;
      }
      break;
    }
    default:
      LOG(FATAL) << "Unknown address family " << address_family;
  }
  return true;
}

bool NetMaskToMaskLength(const IPAddress& address, int* result) {
  int length;
  switch (address.address_family()) {
    case AF_INET: {
      uint32 ipv4 = IPAddressToHostUInt32(address);
      length = ipv4 != 0 ? 32 - Bits::FindLSBSetNonZero(ipv4) : 0;
      // Verify this is a valid netmask.
      if ((~ipv4 & (~ipv4 + 1)) != 0)
        return false;
      break;
    }

    case AF_INET6: {
      absl::uint128 ipv6 = IPAddressToUInt128(address);
      length = ipv6 != 0 ? 128 - Bits::FindLSBSetNonZero128(ipv6) : 0;
      // Verify this is a valid netmask.
      if ((~ipv6 & (~ipv6 + 1)) != 0)
        return false;
      break;
    }

    default:
      return false;
  }

  if (result)
    *result = length;
  return true;
}

bool MaskLengthToIPAddress(int family, int length, IPAddress* address) {
  switch (family) {
    case AF_INET: {
      if (length < 0 || length > 32)
        return false;

      // <<32 on an uint32 is undefined, LL is important.
      uint32 mask = 0xffffffffLL << (32 - length);
      if (address)
        *address = HostUInt32ToIPAddress(mask);
      return true;
    }

    case AF_INET6: {
      if (length < 0 || length > 128)
        return false;

      // uint28 << 128 is undefined, so case on length.
      absl::uint128 mask =
                  length == 0 ? 0 : absl::kuint128max << (128 - length);
      if (address)
        *address = UInt128ToIPAddress(mask);
      return true;
    }
  }
  return false;
}

std::string AddressFamilyToString(int family) {
  switch (family) {
    case AF_UNSPEC:
      return "unspecified";
    case AF_INET:
      return "IPv4";
    case AF_INET6:
      return "IPv6";
  }
  return "unknown";
}

}  //  namespace stratum
