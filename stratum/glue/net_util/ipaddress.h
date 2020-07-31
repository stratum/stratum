// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


// Various classes for storing Internet addresses:
//
//  * IPAddress:      An IPv4 or IPv6 address. Fundamentally represents a host
//                    (or more precisely, an network interface).
//                    Roughly analogous to a struct in_addr.
//  * SocketAddress:  A socket endpoint address (IPAddress, plus a port).
//                    Roughly analogous to a struct sockaddr_in.
//  * IPRange:        A subnet address, ie. a range of IPv4 or IPv6 addresses
//                    (IPAddress, plus a prefix length). (Would have been named
//                    IPSubnet, but that was already taken several other
//                    places.)
//
// The IPAddress class explicitly does not handle mapped or compatible IPv4
// addresses specially. In particular, operator== will treat 1.2.3.4 (IPv4),
// ::1.2.3.4 (compatible IPv4 embedded in IPv6) and
// ::ffff:1.2.3.4 (mapped IPv4 embedded in IPv6) as all distinct.
//
// Note that the old typedef from IPAddress to an uint32 has been renamed to
// OldIPAddress and is deprecated.

#ifndef STRATUM_GLUE_NET_UTIL_IPADDRESS_H_
#define STRATUM_GLUE_NET_UTIL_IPADDRESS_H_

#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

#include "absl/base/port.h"
#include "absl/numeric/int128.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"

namespace stratum {

#ifdef __APPLE__
#define s6_addr16 __u6_addr.__u6_addr16
#define s6_addr32 __u6_addr.__u6_addr32
#endif  // __APPLE__

// Forward declaration for IPAddress::ipv4_address() -- see the full
// explanation below.
class IPAddress;
struct hostent;

// Forward declaration for IPAddress ostream operator, so that DCHECK
// macros know that there is an appropriate overload.
std::ostream& operator<<(std::ostream& stream, const IPAddress& address);

IPAddress GetCoercedIPv4Address(const IPAddress& ip6);

class IPAddress {
 public:
  // Default constructor. Leaves the object in an empty state.
  // The empty state is analogous to a NULL pointer; the only operations
  // that are allowed on the object are:
  //
  //   * Assignment and copy construction.
  //   * Checking for the empty state (address_family() will return AF_UNSPEC,
  //     or equivalently, the helper function IsInitializedAddress() will
  //     return false).
  //   * Comparison (operator== and operator!=).
  //   * Logging (operator<<)
  //   * Ordering (IPAddressOrdering)
  //   * Hashing
  //
  // In particular, no guarantees are made about the behavior of the
  // IPv4/IPv6 conversion accessors, of string conversions and
  // serialization, of IsAnyIPAddress() and friends.
  //
  // Also see the default constructor for SocketAddress, below.
  IPAddress() : address_family_(AF_UNSPEC) {
  }

  // Constructors from standard BSD socket address structures.
  // For conversions from uint32 (for legacy Google3 code) and from strings,
  // see under "Free utility functions", below.
  explicit IPAddress(const in_addr& addr)
      : address_family_(AF_INET) {
    addr_.addr4 = addr;
  }
  explicit IPAddress(const in6_addr& addr)
      : address_family_(AF_INET6) {
    addr_.addr6 = addr;
  }

  // Accessors. Note that these return their value directly even when it
  // is not a primitive type, as this was deemed more logical, and with
  // recent (>= 4.1) gcc is just as fast as using an output parameter.

  // The address family; either AF_UNSPEC, AF_INET or AF_INET6.
  int address_family() const {
    return address_family_;
  }

  // The address as an in_addr structure; CHECK-fails if address_family() is
  // not AF_INET (ie. the held address is not an IPv4 address).
  in_addr ipv4_address() const {
    CHECK(AF_INET == address_family_)
        << "Trying to call ipv4_address() on the IPv6 address "
        << this->ToString();
    return addr_.addr4;
  }

  // The address as an in6_addr structure; CHECK-fails if address_family() is
  // not AF_INET6 (ie. the held address is not an IPv6 address).
  in6_addr ipv6_address() const {
    CHECK(AF_INET6 == address_family_)
        << "Trying to call ipv6_address() on the IPv4 address "
        << this->ToString();
    return addr_.addr6;
  }

  // The address in string form, as returned by inet_ntop(). In particular,
  // this means that IPv6 addresses may be subject to zero compression
  // (e.g. "2001:700:300:1800::f" instead of "2001:700:300:1800:0:0:0:f").
  std::string ToString() const;

  // Returns the same as ToString().
  // <buffer> must have room for at least INET6_ADDRSTRLEN bytes,
  // including the final NUL.
  void ToCharBuf(char *buffer) const;

  // Returns the address as a sequence of bytes in network-byte-order.
  // This is suitable for writing onto the wire or into a protocol buffer
  // as a string (proto1 syntax) or bytes (proto2 syntax).
  // IPv4 will be 4 bytes. IPv6 will be 16 bytes.
  // Can be parsed using PackedStringToIPAddress().
  std::string ToPackedString() const;

  // Static, constant IPAddresses for convenience.
  static IPAddress Any4();       // 0.0.0.0
  static IPAddress Loopback4();  // 127.0.0.1

  static IPAddress Any6();       // ::
  static IPAddress Loopback6();  // ::1

  // IP addresses have no natural ordering, so only equality operators
  // are defined. For using IPAddress elements as keys in STL containers,
  // see IPAddressOrdering, below.
  bool operator==(const IPAddress& other) const;
  bool operator!=(const IPAddress& other) const {
    return !(*this == other);
  }

  // POD type, so no DISALLOW_COPY_AND_ASSIGN:
  // IPAddress(const IPAddress&) = default;
  // IPAddress& operator=(const IPAddress&) = default;

  template <typename H>
  friend H AbslHashValue(H h, const IPAddress& a) {
    auto state = H::combine(std::move(h), a.address_family_);
    switch (a.address_family_) {
    case AF_INET:
      return H::combine(std::move(state), a.addr_.addr4.s_addr);
    case AF_INET6:
      state = H::combine(std::move(state), a.addr_.addr6.s6_addr32[0]);
      state = H::combine(std::move(state), a.addr_.addr6.s6_addr32[1]);
      state = H::combine(std::move(state), a.addr_.addr6.s6_addr32[2]);
      state = H::combine(std::move(state), a.addr_.addr6.s6_addr32[3]);
      return state;
    case AF_UNSPEC:
      // Additional hash input to differentiate AF_UNSPEC from IPAddress::Any4
      return H::combine(std::move(state), "AF_UNSPEC");
    default:
      LOG(FATAL) << "Unknown address family " << a.address_family_;
    }
  }

  friend struct IPAddressOrdering;

 private:
  int address_family_;
  union {
    in_addr addr4;
    in6_addr addr6;
  } addr_;
};

class SocketAddress {
 public:
  // Default constructor. Leaves the object in an empty state.
  // The empty state is analogous to a NULL pointer; the only operations
  // that are allowed on the object are:
  //
  //   * Assignment and copy construction.
  //   * Checking for the empty state (host() will return an
  //     empty IPAddress object, or equivalently, the helper function
  //     IsInitializedSocketAddress() will return false).
  //   * Comparison (operator== and operator!=).
  //   * Logging (operator<<)
  //   * Ordering (SocketAddressOrdering)
  //   * Hashing
  //
  // In particular, no guarantees are made about the behavior of the
  // IPv4/IPv6 conversion accessors, of string conversions and
  // serialization, of ordering using SocketAddress ordering,
  // of hashing, or of the port() accessor.
  //
  // Note that you can also arrive at the empty state by construction
  // from a struct sockaddr, below.
  SocketAddress()
      : port_(0) {
  }

  // Constructor with IP address and port (in host byte order).
  SocketAddress(const IPAddress& host, uint16 port)
      : host_(host), port_(port) {
  }

  // Constructors from standard BSD socket address structures.
  explicit SocketAddress(const struct sockaddr_in& sin)
      : host_(sin.sin_addr), port_(ntohs(sin.sin_port)) {
    CHECK_EQ(AF_INET, sin.sin_family);
  }

  // The following constructors are not recommended for most users, because
  // storing raw ::ffff:0.0.0.0/96 addresses may lead to cumbersome interactions
  // with common google3 libraries.  Instead, consider passing sockaddr_* types
  // directly to NormalizeSocketAddress(), where IPv4-mapped IPv6 addresses will
  // be converted to a plain IPv4 format.
  explicit SocketAddress(const struct sockaddr_in6& sin6)
      : host_(sin6.sin6_addr), port_(ntohs(sin6.sin6_port)) {
    CHECK_EQ(AF_INET6, sin6.sin6_family);
  }
  explicit SocketAddress(const struct sockaddr& saddr);
  explicit SocketAddress(const struct sockaddr_storage& saddr);

  // For conversion from a string, see under "Free utility functions",
  // below.

  // Accessors. Note that these return their value directly even when it
  // is not a primitive type, as this was deemed more logical, and with
  // recent (>= 4.1) gcc is just as fast as using an output parameter.

  // The individual parts of the socket address.
  IPAddress host() const {
    return host_;
  }

  // Port in host byte order.
  uint16 port() const {
    if (host_.address_family() == AF_UNSPEC) {
      LOG(DFATAL) << "Trying to take port() of an empty SocketAddress";
      // port_ is always zero in this case, so we will have a defined
      // return value in opt mode.
    }
    return port_;
  }

  // A string representation of the address.
  //
  // For IPv4 addresses, this is "host:port" (ie. "127.0.0.1:80").
  // For IPv6 addresses, this is "[host]:port" (ie. "[::1]:80").
  //
  // NOTE(sesse): Before you send a CL asking that ToString() on an
  // uninitialized SocketAddress should not CHECK-fail in debug mode,
  // please consider:
  //
  //   1. There's a guarantee that StringToSocketAddress can parse anything
  //      ToString() outputs, and one might not want to make
  //      StringToSocketAddress("") return true.
  //   2. If you just want to log a SocketAddress, do not use ToString();
  //      simply do LOG(INFO) << addr, which can handle empty SocketAddress
  //      objects just fine.
  std::string ToString() const;

  // Returns the address as a sequence of bytes in network-byte-order,
  // IPAddress first, then port. This is suitable for writing onto the
  // wire or into a protocol buffer as a string (proto1 syntax) or
  // bytes (proto2 syntax). IPv4 will be 6 bytes. IPv6 will be 18
  // bytes. Can be parsed using PackedStringToSocketAddress().
  std::string ToPackedString() const;

  // The socket address as a sockaddr_in structure; CHECK-fails if
  // host().address_family() is not AF_INET (ie. the held address is
  // not an IPv4 address).
  sockaddr_in ipv4_address() const {
    sockaddr_in ret;
    memset(&ret, 0, sizeof(ret));
    ret.sin_family = AF_INET;
    ret.sin_addr = host_.ipv4_address();
    ret.sin_port = htons(port_);
    return ret;
  }

  // The socket address as a sockaddr_in6 structure; CHECK-fails if
  // host().address_family() is not AF_INET6 (ie. the held address is
  // not an IPv6 address).
  sockaddr_in6 ipv6_address() const {
    sockaddr_in6 ret;
    memset(&ret, 0, sizeof(ret));
    ret.sin6_family = AF_INET6;
    ret.sin6_addr = host_.ipv6_address();
    ret.sin6_port = htons(port_);
    return ret;
  }

  // Returns the socket address as a sockaddr_storage structure, with sa_family
  // matching the held address_family.  CHECK-fails if the family is not AF_INET
  // or AF_INET6.
  //
  // When interfacing with the kernel, you should typically prefer
  // SocketAddressToFamily(), as it helps avoid address family mismatches.
  sockaddr_storage generic_address() const;

  // Socket addresses have no natural ordering, so only equality operators
  // are defined.
  bool operator==(const SocketAddress& other) const {
    // Compare port first, since it is cheaper.
    // Note that we make use of an internal implementation detail here,
    // namely that port_ is known to always be zero for
    // an empty SocketAddress; thus, use port_ instead of the
    // external interface port() (which CHECK-fails on emptiness).
    return port_ == other.port_ && host_ == other.host_;
  }
  bool operator!=(const SocketAddress& other) const {
    return port_ != other.port_ || host_ != other.host_;
  }

  // POD type, so no DISALLOW_COPY_AND_ASSIGN:
  // SocketAddress(const SocketAddress&) = default;
  // SocketAddress& operator=(const SocketAddress&) = default;

  template <typename H>
  friend H AbslHashValue(H h, const SocketAddress& s) {
    return H::combine(std::move(h), s.host_, s.port_);
  }

 private:
  IPAddress host_;
  uint16 port_;
};

namespace net_util_internal {

// Truncate any IPv4, IPv6, or empty IPAddress to the specified length.
// If *length_io exceeds the number of bits in the address family, then it
// will be overwritten with the correct value.  Normal addresses will
// CHECK-fail if the length is negative, but empty addresses ignore the
// length and write -1.
//
// This is not for external use; see TruncateIPAddress() instead.
IPAddress TruncateIPAndLength(const IPAddress& addr, int* length_io);

// A templated Formatter for use with the strings::Join API to print
// collections of IPAddresses, SocketAddresses, or IPRanges (or anything
// with a suitable ToString() method).  See also //strings/join.h.
// TODO(unknown): Replace with calls to something better in //strings:join, once
// something better is available.
template<typename T>
struct ToStringJoinFormatter {
  void operator()(std::string* out, const T& t) const {
    out->append(t.ToString());
  }
};

}  // namespace net_util_internal

class IPRange {
 public:
  // Default constructor. Leaves the object in an empty state.
  // The empty state is analogous to a NULL pointer; the only operations
  // that are allowed on the object are:
  //
  //   * Assignment and copy construction.
  //   * Checking for the empty state (IsInitializedRange() will return false).
  //   * Comparison (operator== and operator!=).
  //   * Logging (operator<<)
  //   * Ordering (IPRangeOrdering)
  //   * Hashing
  //
  // In particular, no guarantees are made about the behavior of the
  // of string conversions and serialization, or any other accessors.
  IPRange() : length_(-1) {
  }

  // Constructs an IPRange from an address and a length. Properly zeroes out
  // bits and adjusts length as required, but CHECK-fails on negative lengths
  // (since that is inherently nonsensical). Typical examples:
  //
  //   129.240.2.3/10 => 129.192.0.0/10
  //   2001:700:300:1800::/48 => 2001:700:300::/48
  //
  //   127.0.0.1/33 => 127.0.0.1/32
  //   ::1/129 => ::1/128
  //
  //   IPAddress()/* => empty IPRange()
  //
  //   127.0.0.1/-1 => undefined (currently CHECK-fail)
  //   ::1/-1 => undefined (currently CHECK-fail)
  //
  IPRange(const IPAddress& host, int length)
      : host_(net_util_internal::TruncateIPAndLength(host, &length)),
        length_(length) {
  }

  // Unsafe constructor from a host and prefix length.
  //
  // This is the fastest way to construct an IPRange, but the caller must
  // ensure that all inputs are strictly validated:
  //   - IPv4 host must have length 0..32
  //   - IPv6 host must have length 0..128
  //   - The host must be cleanly truncated, i.e. there must not be any bits
  //     set beyond the prefix length.
  //   - Uninitialized IPAddress() must have length -1
  //
  // For performance reasons, these constraints are only checked in debug mode.
  // Any violations will result in undefined behavior.  Callers who cannot
  // guarantee correctness should use IPRange(host, length) instead.
  static IPRange UnsafeConstruct(const IPAddress& host, int length) {
    return IPRange(host, length, /* dummy = */ 0);
  }

  // Construct an IPRange from just an IPAddress, applying the
  // address-family-specific maximum netmask length.
  explicit IPRange(const IPAddress& host)
      : host_(host) {
    switch (host.address_family()) {
      case AF_INET:
        length_ = 32;
        break;
      case AF_INET6:
        length_ = 128;
        break;
      default:
        length_ = -1;
        LOG(DFATAL) << "unknown address family: " << host.address_family();
    }
  }

  // For conversion from a string, see under "Free utility functions",
  // below.

  // Accessors. Note that these return their value directly even when it
  // is not a primitive type, as this was deemed more logical, and with
  // recent (>= 4.1) gcc is just as fast as using an output parameter.

  // The individual parts of the subnet.
  IPAddress host() const {
    return host_;
  }
  int length() const {
    return length_;
  }

  // The bounding IPAddresses in this IPRange.  The "network address"
  // is the "all zeroes" address, or lower bound.  The "broadcast address"
  // is the "all ones" address, or upper bound.
  //
  // Simple example:
  //   IPRange range;
  //   CHECK(StringToIPRangeAndTruncate("10.1.1.0/24", &range));
  //   range.network_address().ToString();    // returns "10.1.1.0"
  //   range.broadcast_address().ToString();  // returns "10.1.1.255"
  IPAddress network_address() const;
  IPAddress broadcast_address() const;

  // Subnets have no natural ordering, so only equality operators
  // are defined.
  bool operator==(const IPRange& other) const {
    // Compare length first, since it is cheaper.
    return length() == other.length() && host() == other.host();
  }
  bool operator!=(const IPRange& other) const {
    return length() != other.length() || host() != other.host();
  }

  // A string representation of the subnet, in "host/length" format.
  // Examples would be "127.0.0.0/8" or "2001:700:300:1800::/64".
  std::string ToString() const;

  // Convert an IPRange into a sequence of bytes suitable for writing to a
  // protocol buffer as a string (proto1 syntax) or bytes (proto2 syntax).
  // Any bits beyond the prefix length will be truncated.
  //
  // This will crash with LOG(FATAL) if the IPRange is uninitialized.
  //
  // The address family and prefix length are stored in the first byte, with
  // values [0..128] assigned to IPv6 and [200..232] assigned to IPv4.  The
  // remaining bytes contain the address, with all trailing zeroes omitted.
  //
  // This table shows the maximum encoded size for some example prefix lengths.
  // IPv4 and IPv6 follow the same formula.
  //
  //   Prefix length (bits)  Maximum output length (bytes)
  //   --------------------  -----------------------------
  //   /0                    1 + 0 = 1
  //   /17 .. /24            1 + 3 = 4
  //   /25 .. /32            1 + 4 = 5
  //   /41 .. /48            1 + 6 = 7
  //   /57 .. /64            1 + 8 = 9
  //   /N                    1 + ceil(N/8)
  std::string ToPackedString() const;

  // Convenience ranges, representing every IPv4 or IPv6 address.
  static IPRange Any4() {
    return IPRange::UnsafeConstruct(IPAddress::Any4(), 0);  // 0.0.0.0/0
  }
  static IPRange Any6() {
    return IPRange::UnsafeConstruct(IPAddress::Any6(), 0);  // ::/0
  }

  template <typename H>
  friend H AbslHashValue(H h, const IPRange& r) {
    return H::combine(std::move(h), r.host_, r.length_);
  }

  // POD type, so no DISALLOW_COPY_AND_ASSIGN:
  // IPRange(const IPRange&) = default;
  // IPRange& operator=(const IPRange&) = default;

 private:
  // Internal implementation of UnsafeConstruct().
  IPRange(const IPAddress& host, int length, int dummy)
      : host_(host), length_(length) {
    DCHECK_EQ(host_, net_util_internal::TruncateIPAndLength(host, &length))
        << "Host has bits set beyond the prefix length.";
    DCHECK_EQ(length_, length)
        << "Length is inconsistent with address family.";
  }

  IPAddress host_;
  int length_;
};

// Free utility functions for IPAddress.

inline IPAddress UInt32ToIPAddress(uint32 bytes) {
  // Do NOT implement this in depot3.
  LOG(FATAL) << __FUNCTION__
             << " deprecated and not supported in depot3 sandcastle";

  return IPAddress();
}

// Convert a host byte order uint32 into an IPv4 IPAddress.
//
// This is the less-evil cousin of UInt32ToIPAddress.  It can be used with
// protobufs, mathematical/bitwise operations, or any other case where the
// address is represented as an ordinary number.
//
// Example usage:
//   HostUInt32ToIPAddress(0x01020304).ToString();  // Yields "1.2.3.4"
//
inline IPAddress HostUInt32ToIPAddress(uint32 address) {
  in_addr addr;
  addr.s_addr = htonl(address);
  return IPAddress(addr);
}

// Convert an IPv4 IPAddress to a uint32 in host byte order.
// This is the inverse of HostUInt32ToIPAddress().
//
// Example usage:
//   const IPAddress addr(StringToIPAddressOrDie("1.2.3.4"));
//   IPAddressToHostUInt32(addr);  // Yields 0x01020304
//
// Will CHECK-fail if addr does not contain an IPv4 address.
inline uint32 IPAddressToHostUInt32(const IPAddress &addr) {
  return ntohl(addr.ipv4_address().s_addr);
}

// Convert a uint128 in host byte order to an IPv6 IPAddress
// (e.g., uint128(0, 1) will become "::1").
// Not a constructor, to make it easier to grep for the ugliness later.
IPAddress UInt128ToIPAddress(const absl::uint128& bigint);

// Convert an IPv6 IPAddress to a uint128 in host byte order
// (e.g., "::1" will become uint128(0, 1)).
// Will CHECK-fail if addr does not contain an IPv6 address,
// so use with care, and only in low-level code.
inline absl::uint128 IPAddressToUInt128(const IPAddress& addr) {
  struct in6_addr addr6 = addr.ipv6_address();
  return absl::MakeUint128(
      static_cast<uint64>(ntohl(addr6.s6_addr32[0])) << 32 |
          static_cast<uint64>(ntohl(addr6.s6_addr32[1])),
      static_cast<uint64>(ntohl(addr6.s6_addr32[2])) << 32 |
          static_cast<uint64>(ntohl(addr6.s6_addr32[3])));
}

// Parse an IPv4 or IPv6 address in textual form to an IPAddress.
// Not a constructor since it can fail (in which case it returns false,
// and the contents of "out" is undefined). If only validation is required,
// "out" can be set to nullptr.
//
// The input argument can be in whatever form inet_pton(AF_INET4, ...) or
// inet_pton(AF_INET6, ...) accepts (ie. typically something like "127.0.0.1"
// or "2001:700:300:1800::f").
//
// Note that in particular, this function does not do DNS lookup.
//
bool StringToIPAddress(const char* str, IPAddress* out) ABSL_MUST_USE_RESULT;
inline bool StringToIPAddress(const std::string& str,
                              IPAddress* out) ABSL_MUST_USE_RESULT;
inline bool StringToIPAddress(const std::string& str, IPAddress* out) {
  return StringToIPAddress(str.c_str(), out);
}

// StringToIPAddress conversion methods that CHECK()-fail on invalid input.
// Not a good idea to use on user-provided input.
inline IPAddress StringToIPAddressOrDie(const char* str) {
  IPAddress ip;
  CHECK(StringToIPAddress(str, &ip)) << "Invalid IP " << str;
  return ip;
}
inline IPAddress StringToIPAddressOrDie(const std::string& str) {
  return StringToIPAddressOrDie(str.c_str());
}

// Parse a "binary" or packed string containing an IPv4 or IPv6 address in
// non-textual, network-byte-order form to an IPAddress.  Not a constructor
// since it can fail (in which case it returns false, and the contents of
// "out" is undefined). If only validation is required, "out" can be set to
// nullptr.
bool PackedStringToIPAddress(const std::string& src, IPAddress* out);

// Binary packed string conversion methods that CHECK()-fail on invalid input.
inline IPAddress PackedStringToIPAddressOrDie(const std::string& str) {
  IPAddress ip;
  CHECK(PackedStringToIPAddress(str, &ip))
      << "Invalid packed IP address of length " << str.length();
  return ip;
}
inline IPAddress PackedStringToIPAddressOrDie(const char* str, size_t len) {
  return PackedStringToIPAddressOrDie(std::string(str, len));
}

// A free function to parse hexadecimal strings like
// "fe80000000000000000573fffea00065" in to an IPv6 IPAddress.  @ip6 may be
// NULL, in which case the function still behaves as a boolean test for
// the validity of a given string being converted to an IPv6 IPAddress.
bool ColonlessHexToIPv6Address(const std::string& hex_str, IPAddress* ip6);

// For debugging/logging. Note that as a special case, you can log an
// uninitialized IP address, although you cannot use ToString() on it.
inline std::ostream& operator<<(std::ostream& stream,
                                const IPAddress& address) {
  if (address.address_family() == AF_UNSPEC) {
    return stream << "<uninitialized IPAddress>";
  } else {
    return stream << address.ToString();
  }
}

// Free function to return a version of the address in string form
// (a la IPAddress::ToString()) which additionally conforms to URI
// guidelines, e.g. RFC 3986 section 3.2.2.  Specifically, IPv4
// addresses remain unchanged while IPv6 addresses are encapsulated
// within '[' and ']'.
std::string IPAddressToURIString(const IPAddress& ip);

// Free function to output the PTR representation of an IPAddress.
// IPv4 and IPv6 addresses are output in their in-addr.arpa and
// ip6.arpa formats respectively.
std::string IPAddressToPTRString(const IPAddress& ip);

// Free function to return an IPAddress from the corresponding PTR
// representation, e.g. the inverse of IPAddressToPtrString.
bool PTRStringToIPAddress(const std::string& ptr_address,
                          IPAddress* out) ABSL_MUST_USE_RESULT;

// Specific JoinFormatters for IPAddress, SocketAddress, and IPRange.
typedef net_util_internal::ToStringJoinFormatter<IPAddress>
        IPAddressJoinFormatter;
typedef net_util_internal::ToStringJoinFormatter<SocketAddress>
        SocketAddressJoinFormatter;
typedef net_util_internal::ToStringJoinFormatter<IPRange> IPRangeJoinFormatter;

// Boolean convenience checks.
bool IsAnyIPAddress(const IPAddress& ip);

// Return true if 'ip' is 127.0.0.1 or ::1. Returns false if 'ip' is any other
// IPv4 or IPv6 address. Otherwise the behavior is undefined.
// Note that this function returns false for all addresses in the 127.0.0.0/8
// subnet except for 127.0.0.1, even though every address in that subnet is a
// loopback address. For this reason, most people should probably be calling
// IsLoopbackIPAddress() instead.
bool IsCanonicalLoopbackIPAddress(const IPAddress& ip);

// Return true if 'ip' is in the 127.0.0.0/8 range, or if it is ::1. Returns
// false if 'ip' is any other IPv4 or IPv6 address. Otherwise the behavior is
// undefined.
bool IsLoopbackIPAddress(const IPAddress& ip);

// Choose a random (IPv4 or IPv6) address from a host name lookup.
IPAddress ChooseRandomAddress(const hostent* hp);

// Choose a random IPAddress from vector of same.
IPAddress ChooseRandomIPAddress(const std::vector<IPAddress> *ipvec);

// Returns whether the address is initialized or not.
inline bool IsInitializedAddress(const IPAddress& addr) {
  return addr.address_family() != AF_UNSPEC;
}

// Return the family-dependent length (in bits) of an IP address given an
// IPAddress object.  A debug-fatal error is logged if the address family
// is not of the Internet variety, i.e. not one of set(AF_INET, AF_INET6);
// the caller is responsible for verifying IsInitializedAddress(ip).
inline int IPAddressLength(const IPAddress &ip) {
  switch (ip.address_family()) {
    case AF_INET:
      return 32;
    case AF_INET6:
      return 128;
    default:
      LOG(DFATAL)
          << "IPAddressLength() of object with invalid address family: "
          << ip.address_family();
      return -1;
  }
}

// A functor for using IPAddress objects as members of a set<>, map<>, etc..
// For use in hash tables, hash<IPAddress> is also defined below.
//
// The ordering defined by this functor is: First uninitialized
// addresses, then all IPv4 addresses, then all IPv6
// addresses. Internally, the addresses are ordered lexically by
// network byte order (so they go 0.0.0.0, 0.0.0.1, 0.0.0.2, etc.).
struct IPAddressOrdering {
  bool operator() (const IPAddress& lhs, const IPAddress& rhs) const;
};

// A functor for using SocketAddress objects as members of a set<>, map<>, etc..
// For use in hash tables, hash<SocketAddress> is also defined below.
//
// The ordering defined by this functor is:
// First the IPAddress, then the port number.
struct SocketAddressOrdering {
  bool operator() (const SocketAddress& lhs, const SocketAddress& rhs) const;
};

// A functor for using IPRange objects as members of a set<>, map<>, etc..
// For use in hash tables, hash<IPRange> is also defined below.
//
// The ordering defined by this functor is:
// First the network address (as defined by IPAddressOrdering),
// then the prefix length.
struct IPRangeOrdering {
  bool operator() (const IPRange& lhs, const IPRange& rhs) const;
};

// IPv6-specific functions for different classes of addresses. These
// can optionally populate an IPAddress with the source IPv4 address.  In
// the case of a Teredo address much more information can be extracted.
// In the below examples X.Y.Z.Q is assumed to represent an IPv4 address.

// Compatible IPv4 addresses are of the form "::X.Y.Z.Q/96".
bool GetCompatIPv4Address(const IPAddress& ip6, IPAddress* ip4);

// Mapped IPv4 addresses are of the form "::ffff:X.Y.Z.Q/96".
bool GetMappedIPv4Address(const IPAddress& ip6, IPAddress* ip4);

// 6to4 addresses are of the form:
//
//     2002:UpperV4Hex:LowerV4Hex::/48
//
// i.e. 2002::/16 plus the 32 bit IPv4 address totaling 48 bits.
// So for the IPv4 address 1.2.3.4, the 6to4 /48 block routed to it
// is 2002::0102:0304::/48.

// Extracts the embedded IPv4 address from a 6to4 address, if possible.
// For example, 2002:c000:201:: yields 192.0.2.1.
bool Get6to4IPv4Address(const IPAddress& ip6, IPAddress* ip4);

// Converts any IPv4 range into its 6to4 equivalent.
// For example, 192.0.2.4/31 yields 2002:c000:204::/47.
bool Get6to4IPv6Range(const IPRange& iprange4, IPRange* iprange6);

// ISATAP addresses have a lower 64 bits of the form:
//
//     <...>:0[0-3]00:5efe:ClientUpperV4Hex:ClientLowerV4Hex
//
// See:
//     http://tools.ietf.org/html/rfc5214#section-6.1
//
// NOTE: ISATAP is one of the transition mechanisms that does NOT require
// verifiably valid IPv4 routing to work.  For such protocols, e.g 6to4
// and Teredo, _some_ level of trust can be established because Google's
// view of global routing would need to be spoofed in order for the TCP
// connection to complete.  (Aside: For Teredo, a malicious server/relay
// could be used, but there are other ways to mitigate this particular
// risk, specifically: having a Google owned and operated server/relay.)
//
// Because the client address is in the lower 64 bits and this is already
// considered to under the control of leaf networks/nodes all that's
// required is a valid IPv6 route and the rest is trivially spoofable.
// Hence ISATAP addresses SHOULD NOT be considered within a security
// context.  This function is here to make unverifiable ISATAP detection
// possible.
bool GetIsatapIPv4Address(const IPAddress& ip6, IPAddress* ip4);

// Teredo addresses are of the form:
//
//     2001:0:ServerUpperV4Hex:ServerLowerV4Hex:
//       flags:~ClientUDPPort:~ClientUpperV4Hex:~ClientLowerV4Hex
//
// Yes, it's complicated.  For eye-bleeding details see:
//     http://tools.ietf.org/html/rfc4380#section-4
bool GetTeredoInfo(const IPAddress& ip6, IPAddress* server, uint16* flags,
                   uint16* port, IPAddress* client);

// Extract the embedded IPv4 client address, if present.  This only
// returns true if the address is one of [compat, mapped, 6to4, teredo],
// false otherwise.  Due to the spoof-ability of these addresses on the
// wire this should NEVER be used in a security context (e.g. to evaluate
// or elevate privileges).  ISATAP addresses are explicitly excluded.
bool GetEmbeddedIPv4ClientAddress(const IPAddress& ip6, IPAddress *ip4);

inline IPAddress GetCoercedIPv4Address(const IPAddress& ip6) {
  // Do NOT implement this in depot3.
  LOG(FATAL) << __FUNCTION__ << " not supported in depot3 sandcastle";
}

// Normalizes the address representation with respect to IPv4 addresses -- that
// is, mapped IPv4 addresses ("::ffff:X.Y.Z.Q") are converted to pure IPv4
// addresses.  All other IPv4, IPv6, and empty values are left unchanged.
//
// For more discussion of mapped addresses and how/when an application
// can receive them from the OS see:
//
//     http://tools.ietf.org/html/rfc4038#section-4.2
//
// NOTE: IPv4-Compatible IPv6 Addresses (a.k.a. "compat" addresses) are
// not normalized here.  A "compat" address will not be passed to an
// application by the OS unless such traffic actually arrives at the node.
// If an application is seeing "compat" addresses then an investigation
// into the origin of the traffic should be initiated.  See also:
//
//     http://tools.ietf.org/html/rfc4291#section-2.5.5.1
IPAddress NormalizeIPAddress(const IPAddress& ip);

// Returns an address suitable for use in IPv6-aware contexts.  This is the
// opposite of NormalizeIPAddress() above.  IPv4 addresses are converted into
// their IPv4-mapped address equivalents (e.g. 192.0.2.1 becomes
// ::ffff:192.0.2.1).  IPv6 addresses are a noop (they are returned unchanged).
// Additionally, 0.0.0.0 will be mapped to ::, to ease bind() before listen().
// Another type of address (e.g. AF_UNSPEC) will result in a CHECK-failure.
//
// Historically, using const sockaddr_in* with a dualstack socket (i.e.
// an AF_INET6 socket without the IPV6_V6ONLY socket option set) mostly
// mostly "just worked".  However, per:
//
//     http://tools.ietf.org/html/rfc3493#section-3.7
//
// (discussion of the use of IPv4-mapped address with dualstack sockets),
// this behaviour is not technically required.  Nowadays EINVAL is returned.
//
// Dualstack socket applications should use NormalizeIPAddress()
// (or NormalizeSocketAddress(), below) when reading addresses from the
// Berkeley/POSIX sockets API and use DualstackIPAddress() (or
// DualstackSocketAddress(), below) when writing addresses to same.
IPAddress DualstackIPAddress(const IPAddress& ip);

// Free utility functions for SocketAddress.

//
// Parse an IPv4 or IPv6 address in textual form to a SocketAddress.
// Not a constructor since it can fail (in which case it returns false,
// and the contents of "out" is undefined). If only validation is required,
// "out" can be set to nullptr.
//
// The format accepted is the same that is output by SocketAddress::ToString().
//
bool StringToSocketAddress(const std::string& str,
                           SocketAddress* out) ABSL_MUST_USE_RESULT;

// StringToSocketAddress conversion methods that CHECK()-fail on invalid input.
// Not a good idea to use on user provided input.
inline SocketAddress StringToSocketAddressOrDie(const std::string& str) {
  SocketAddress addr;
  CHECK(StringToSocketAddress(str, &addr)) << "Invalid SocketAddress " << str;
  return addr;
}

// Similar to StringToSocketAddress, but allows the port be omitted, in which
// case the passed-in default port value is used.
bool StringToSocketAddressWithDefaultPort(
    const std::string& str, uint16 default_port,
    SocketAddress* out) ABSL_MUST_USE_RESULT;

// Normalizes the host part of an IPv6 SocketAddress.  All other values are left
// unchanged.  See NormalizeIPAddress for more information.
inline SocketAddress NormalizeSocketAddress(const SocketAddress& addr) {
  return addr.host().address_family() == AF_INET6 ?
      SocketAddress(NormalizeIPAddress(addr.host()), addr.port()) :
      addr;
}

// Normalize and construct a SocketAddress from a sockaddr_* in one step.
// This is essentially the inverse of SocketAddressToFamily().
inline SocketAddress NormalizeSocketAddress(const sockaddr& addr) {
  return NormalizeSocketAddress(SocketAddress(addr));
}
inline SocketAddress NormalizeSocketAddress(const sockaddr_in6& addr) {
  return NormalizeSocketAddress(SocketAddress(addr));
}
inline SocketAddress NormalizeSocketAddress(const sockaddr_storage& addr) {
  return NormalizeSocketAddress(SocketAddress(addr));
}

inline SocketAddress DualstackSocketAddress(const SocketAddress& addr) {
  return SocketAddress(DualstackIPAddress(addr.host()), addr.port());
}

// Converts a SocketAddress into a sockaddr_storage of the desired address
// family, suitable for sending to kernel syscalls like connect(), bind(),
// etc.  Returns true if successful.
//
// This is effectively the inverse of NormalizeSocketAddress().
//
// Args:
//   output_family should be one of three values:
//     AF_INET: Builds a sockaddr_in structure.  The held address_family must
//         be AF_INET to avoid an error result.  As a special case, this also
//         maps :: to 0.0.0.0.
//     AF_INET6: Builds a sockaddr_in6 structure.  The held address_family
//         must either be AF_INET or AF_INET6 to avoid an error result.
//         For compatibility with dualstack sockets, any IPv4 address will be
//         mapped to IPv6 using DualstackIPAddress().
//     AF_UNSPEC: Automatically select either AF_INET or AF_INET6, depending
//         on the held address_family.  Use of this mode is discouraged,
//         because it doesn't interact conveniently with dualstack sockets.
//   sa: The SocketAddress to convert.
//   addr_out: Stores the resulting address.  Must not be NULL.
//   addr_size: Stores the number of bytes produced.  NULL is permitted.
//
// This function will never CHECK-fail; errors are signaled by returning false.
// To prevent accidental misuse, this also writes a nonsensical address family
// with a size of zero.
//
// Example usage:
//   int fd = socket(...);
//   SocketAddress addr = ...;
//   sockaddr_storage kaddr;
//   socklen_t kaddr_size;
//   // Note: If the address family is already known from context, then
//   //       there's no need to call GetSocketFamily().
//   CHECK(SocketAddressToFamily(GetSocketFamily(fd), addr,
//                               &kaddr, &kaddr_size));
//   if (connect(fd, reinterpret_cast<sockaddr*>(&kaddr), kaddr_size) == -1) {
//     // Handle the failure.
//   }
//
bool SocketAddressToFamily(int output_family, const SocketAddress& sa,
                           sockaddr_storage* addr_out,
                           socklen_t* size_out) ABSL_MUST_USE_RESULT;

// This behaves like SocketAddressToFamily, with the exception that converting
// 0.0.0.0 to AF_INET6 yields ::, rather than ::ffff:0.0.0.0.  This should be
// used with the bind() syscall, in cases where it's desirable to interpret
// 0.0.0.0 as "all IP addresses".
//
// However, this is not suitable for sending packets, because :: behaves like
// an IPv6 loopback destination, and IPv6 packets cannot be received by AF_INET
// sockets bound to 0.0.0.0.
bool SocketAddressToFamilyForBind(int output_family, const SocketAddress& sa,
                                  sockaddr_storage* addr_out,
                                  socklen_t* size_out) ABSL_MUST_USE_RESULT;

// For debugging/logging. Note that as a special case, you can log an
// uninitialized socket address, although you cannot use ToString() on it.
inline std::ostream& operator<<(std::ostream& stream,
                                const SocketAddress& address) {
  if (address.host().address_family() == AF_UNSPEC) {
    return stream << "<uninitialized SocketAddress>";
  } else {
    return stream << address.ToString();
  }
}

// Returns whether the socket address is initialized or not.
inline bool IsInitializedSocketAddress(const SocketAddress& addr) {
  return IsInitializedAddress(addr.host());
}

// Free utility functions for IPRange.

//
// Parse an IPv4 or IPv6 subnet mask in textual form into an IPRange.
// Not a constructor since it can fail (in which case it returns false,
// and the contents of "out" is undefined). If only validation is required,
// "out" can be set to nullptr.
//
// Note that an improperly zeroed out mask (say, 192.168.0.0/8) will be
// rejected as invalid by this function. If you instead want the excess bits to
// be zeroed out silently, see StringToIPRangeAndTruncate(), below.
//
// The format accepted is the same that is output by IPRange::ToString().
// Any IP addresses without a "/netmask" will be given an implicit
// CIDR netmask length equal to the number of bits in the address
// family (e.g. /32 or /128).  Additionally, IPv4 ranges may have a netmask
// specifier in the older dotted quad format, e.g. "/255.255.0.0".
//
bool StringToIPRange(const std::string& str, IPRange* out) ABSL_MUST_USE_RESULT;

// StringToIPRange conversion methods that CHECK()-fail on invalid input.
// Not a good idea to use on user-provided input.
inline IPRange StringToIPRangeOrDie(const std::string& str) {
  IPRange ipr;
  CHECK(StringToIPRange(str, &ipr)) << "Invalid IP range " << str;
  return ipr;
}

//
// The same as StringToIPRange and StringToIPRangeOrDie, but truncating instead
// of returning an error in the event of an improperly zeroed out mask (ie.,
// 192.168.0.0/8 will automatically be changed to 192.0.0.0/8).
//
bool StringToIPRangeAndTruncate(const std::string& str,
                                IPRange* out) ABSL_MUST_USE_RESULT;
inline IPRange StringToIPRangeAndTruncateOrDie(const std::string& str) {
  IPRange ipr;
  CHECK(StringToIPRangeAndTruncate(str, &ipr)) << "Invalid IP range " << str;
  return ipr;
}

// Truncate any IPv4 or IPv6 address to the specified number of bits.
// Large lengths become no-ops, but negative lengths will CHECK-fail.
inline IPAddress TruncateIPAddress(const IPAddress& addr, int length) {
  CHECK(IsInitializedAddress(addr));
  return net_util_internal::TruncateIPAndLength(addr, &length);
}

// This function is deprecated.  Use the constructor instead.
inline IPRange TruncatedAddressToIPRange(const IPAddress& host, int length) {
  return IPRange(host, length);
}

// Parse a "binary" or packed string containing a *truncated* IPRange of IPv4
// or IPv6 addresses. If this string contains an IPRange that is not truncated
// then it will return false. See IPRange::ToPackedString for more information.
bool PackedStringToIPRange(const std::string& str,
                           IPRange* out) ABSL_MUST_USE_RESULT;

// Returns true if and only if the IP range is initialized, i.e. its
// address is initialized.
inline bool IsInitializedRange(const IPRange& range) {
  return IsInitializedAddress(range.host());
}

// For debugging/logging.
inline std::ostream& operator<<(std::ostream& stream, const IPRange& range) {
  if (!IsInitializedRange(range)) {
    return stream << "<uninitialized IPRange>";
  } else {
    return stream << range.ToString();
  }
}

// Checks whether the given IP address "needle" is within the IP range
// "haystack".  Note that an IPv4 address is never considered to be within an
// IPv6 range, and vice versa.
inline bool IsWithinSubnet(const IPRange& haystack, const IPAddress& needle) {
  return haystack == IPRange(needle, haystack.length());
}

// Checks whether the given IP range "needle" is properly contained within
// the IP range "haystack", i.e. whether "needle" is a more specific of
// "haystack".  Note that an IPv4 range is never considered to be contained
// within an IPv6 range, and vice versa.
inline bool IsProperSubRange(const IPRange& haystack, const IPRange& needle) {
  return haystack.length() < needle.length() &&
      IsWithinSubnet(haystack, needle.host());
}

// Returns true if and only if the IP range is initialized and valid, i.e. its
// address is initialized, its prefix length is valid and the host bits of the
// address are properly zeroed out.
inline bool IsValidRange(const IPRange& range) {
  if (!IsInitializedAddress(range.host())) {
    return false;
  }
  // This branch is arguably unnecessary.  It should only fail in the event of
  // memory corruption, or improper use of UnsafeConstruct().
  const int max_len = IPAddressLength(range.host());
  return 0 <= range.length() && range.length() <= max_len &&
      range == IPRange(range.host(), range.length());
}

// Computes the non-overlapping adjacent IP subnet ranges that cover the IP
// address interval [first_addr, last_addr] and does not cover any other IP
// addresses.  The resulting IP subnet ranges are ordered according to
// IPRangeOrdering and returned in covering_subnets.
//
// The method clears the covering_subnets and returns false if:
//   - first_addr and last_addr does not belong to the same address family, or
//   - first_addr > last_addr according to IPRangeOrdering.
bool IPAddressIntervalToSubnets(const IPAddress& first_addr,
                                const IPAddress& last_addr,
                                std::vector<IPRange>* covering_subnets);

// Return true if the size of the range is greater than the given number.
// CHECK-fail if the IPRange has not been initialized.
bool IsRangeIndexValid(const IPRange& range, absl::uint128 index);

// Return the nth IPAddress in the range. 0 indexes the first one.
// CHECK-fail if the index is out of range or if the range hasn't been
// initialized.  This can effectively be used as an iterator over an
// IPRange with the following pattern:
//
//   for (int i = 0; IsRangeIndexValid(range, i); ++i) {
//     IPAddress addr = NthAddressInRange(range, i);
//     ...
//   }
//
IPAddress NthAddressInRange(const IPRange& range, absl::uint128 index);

// Finds the index of the IP address in the given range.
// CHECK-fails if the IP address does not sit in the given range or if range is
// an invalid IPRange.
//
// This function is the inverse of NthAddressInRange:
//
//   IndexInRange(range, NthAddressInRange(range, i)) == i
absl::uint128 IndexInRange(const IPRange& range, const IPAddress& ip);

// Converts a mask length to an IPAddress. For example, 24 for
// AF_INET is converted to 255.255.255.0, same for IPv6 addresses.
// Useful to convert back and forth between CIDR notation.
// Returns false if the family is unknown or if the length is
// invalid for the family specified. In both cases, address is
// not modified.
bool MaskLengthToIPAddress(int family, int length,
                           IPAddress* address) ABSL_MUST_USE_RESULT;

// Computes the length of a netmask. For example, '255.255.255.0'
// is converted to 24, and returned in the length parameter.
// Useful to convert back and forth between CIDR notation.
//
// Returns false if the address family is not supported, or the
// supplied address does not look like a valid netmask.
// length can be null, in which case the address is still verified.
bool NetMaskToMaskLength(const IPAddress& address,
                         int* length) ABSL_MUST_USE_RESULT;

// If (n > 0), it returns the nth IP address after the given addr.
// If (n < 0), it returns the nth IP address before the given addr.
// For example:
//   "10.1.1.150" + 1 returns "10.1.1.151"
//   "10.1.1.150" + 150 returns "10.1.2.44"
//   "10.1.1.1" - 2 returns "10.1.0.255"
// The method CHECK-fails if the given addr has not been initialized.
// It returns false iff the result crosses the IP address space, in which case
// the contents of "result" is undefined.
// For example, "192.0.0.0" + 0x40000000.
bool IPAddressPlusN(const IPAddress& addr, int n,
                    IPAddress* result) ABSL_MUST_USE_RESULT;

// Subtract the IP range "sub_range" from the less specific IP range "range"
// and return the resulting collection of disjoint IP ranges in "diff_range".
//
// Collectively, the IP ranges in "diff_range" contain the same set of IP
// addresses that "range" does except the ones that are also contained by
// "sub_range". Note that all IP ranges in "diff_range" are more specific
// than "range".
//
// The subtract operation is undefined if "sub_range" is not a more specific
// of "range", in which case "diff_range" is cleared and the method returns
// false. Otherwise, the method returns true and "diff_range" is set to the
// result.
//
// An illustrative example using 8-bit IP addressing:
//   range:      b7  b6  b5  b4  --  --  --  -- /4
//   sub_range:  b7  b6  b5  b4  b3  b2  b1  b0 /8
//
//   diff_range: b7  b6  b5  b4  b3  b2  b1 ~b0 /8
//               b7  b6  b5  b4  b3  b2 ~b1  -- /7
//               b7  b6  b5  b4  b3 ~b2  --  -- /6
//               b7  b6  b5  b4 ~b3  --  --  -- /5
//
bool SubtractIPRange(const IPRange& range,
                     const IPRange& sub_range,
                     std::vector<IPRange>* diff_range);

// Returns a human-readable representaion of the address family.
// Use only for human consumption (e.g. debugging).
std::string AddressFamilyToString(int family);

}  //  namespace stratum

#endif  // STRATUM_GLUE_NET_UTIL_IPADDRESS_H_
