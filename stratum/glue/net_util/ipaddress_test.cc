// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


// Tests for IPAddress and SocketAddress classes.

#include "third_party/stratum/glue/net_util/ipaddress.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

#include <algorithm>
#include <hash_set>
#include <iosfwd>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "third_party/stratum/glue/logging.h"
#include "testing/base/public/gunit.h"
#include "third_party/absl/base/integral_types.h"
#include "third_party/absl/numeric/int128.h"
#include "third_party/absl/strings/substitute.h"

namespace stratum {

#define ARRAYSIZE(a) \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

#define HAVE_SCOPEDMOCKLOG 0
#define HAVE_FIXEDARRAY 0
#define HAVE_VECTOR_CHECKEQ 0

#if HAVE_SCOPEDMOCKLOG
using testing::ScopedMockLog;
#endif

using __gnu_cxx::hash_set;

// Tests for IPAddress.
TEST(IPAddressTest, BasicTests) {
  in_addr addr4;
  in6_addr addr6;

  inet_pton(AF_INET, "1.2.3.4", &addr4);
  inet_pton(AF_INET6, "2001:700:300:1800::f", &addr6);

  IPAddress addr(addr4);
  in_addr returned_addr4 = addr.ipv4_address();
  ASSERT_EQ(AF_INET, addr.address_family());
  EXPECT_EQ(0, memcmp(&addr4, &returned_addr4, sizeof(addr4)));

  addr = IPAddress(addr6);
  in6_addr returned_addr6 = addr.ipv6_address();
  ASSERT_EQ(AF_INET6, addr.address_family());
  EXPECT_EQ(0, memcmp(&addr6, &returned_addr6, sizeof(addr6)));

  addr = IPAddress();
  ASSERT_EQ(AF_UNSPEC, addr.address_family());
}

TEST(IPAddressTest, ToAndFromString4) {
  const std::string kIPString = "1.2.3.4";
  const std::string kBogusIPString = "1.2.3.256";
  const std::string kPTRString = "4.3.2.1.in-addr.arpa";
  in_addr addr4;
  CHECK_GT(inet_pton(AF_INET, kIPString.c_str(), &addr4), 0);

  IPAddress addr;
  EXPECT_FALSE(StringToIPAddress(kBogusIPString, NULL));
  EXPECT_FALSE(StringToIPAddress(kBogusIPString, &addr));
  ASSERT_TRUE(StringToIPAddress(kIPString, NULL));
  ASSERT_TRUE(StringToIPAddress(kIPString, &addr));

  in_addr returned_addr4 = addr.ipv4_address();
  EXPECT_EQ(AF_INET, addr.address_family());
  EXPECT_EQ(0, memcmp(&addr4, &returned_addr4, sizeof(addr4)));

  std::string packed = addr.ToPackedString();
  EXPECT_EQ(sizeof(addr4), packed.length());
  EXPECT_EQ(0, memcmp(packed.data(), &addr4, sizeof(addr4)));

  EXPECT_TRUE(PackedStringToIPAddress(packed, NULL));
  IPAddress unpacked;
  EXPECT_TRUE(PackedStringToIPAddress(packed, &unpacked));
  EXPECT_EQ(addr, unpacked);

  EXPECT_EQ(kIPString, addr.ToString());
  EXPECT_EQ(kIPString, IPAddressToURIString(addr));
  EXPECT_EQ("4.3.2.1.in-addr.arpa", IPAddressToPTRString(addr));
  EXPECT_TRUE(PTRStringToIPAddress(kPTRString, &addr));
  EXPECT_EQ(kIPString, addr.ToString());
}

TEST(IPAddressTest, UnsafeIPv4Strings) {
  // These IPv4 std::string literal formats are supported by inet_aton(3).
  // They are one source of "spoofed" addresses in URLs and generally
  // considered unsafe.  We explicitly do not support them
  // (thankfully inet_pton(3) is significantly more sane).
  const char* kUnsafeIPv4Strings[] = {
    "016.016.016.016",      // 14.14.14.14
    "016.016.016",          // 14.14.0.14
    "016.016",              // 14.0.0.14
    "016",                  // 0.0.0.14
    "0x0a.0x0a.0x0a.0x0a",  // 10.10.10.10
    "0x0a.0x0a.0x0a",       // 10.10.0.10
    "0x0a.0x0a",            // 10.0.0.10
    "0x0a",                 // 0.0.0.10
    "42.42.42",             // 42.42.0.42
    "42.42",                // 42.0.0.42
    "42",                   // 0.0.0.42
  };

  IPAddress ip;
  for (size_t i = 0; i < ARRAYSIZE(kUnsafeIPv4Strings); ++i) {
    EXPECT_FALSE(StringToIPAddress(kUnsafeIPv4Strings[i], &ip));
  }
}

TEST(IPAddressTest, ToAndFromString6) {
  const std::string kIPString = "2001:700:300:1800::f";
  const std::string kIPLiteral = "[2001:700:300:1800::f]";
  const std::string kBogusIPString = "2001:700:300:1800:1:2:3:4:5";
  const std::string kPTRString =
      "f.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0."
      "0.0.8.1.0.0.3.0.0.0.7.0.1.0.0.2.ip6.arpa";

  in6_addr addr6;
  CHECK_GT(inet_pton(AF_INET6, kIPString.c_str(), &addr6), 0);

  IPAddress addr;
  EXPECT_FALSE(StringToIPAddress(kBogusIPString, NULL));
  EXPECT_FALSE(StringToIPAddress(kBogusIPString, &addr));
  ASSERT_TRUE(StringToIPAddress(kIPString, NULL));
  ASSERT_TRUE(StringToIPAddress(kIPString, &addr));

  in6_addr returned_addr6 = addr.ipv6_address();
  EXPECT_EQ(AF_INET6, addr.address_family());
  EXPECT_EQ(0, memcmp(&addr6, &returned_addr6, sizeof(addr6)));

  std::string packed = addr.ToPackedString();
  EXPECT_EQ(sizeof(addr6), packed.length());
  EXPECT_EQ(0, memcmp(packed.data(), &addr6, sizeof(addr6)));

  EXPECT_TRUE(PackedStringToIPAddress(packed, NULL));
  IPAddress unpacked;
  EXPECT_TRUE(PackedStringToIPAddress(packed, &unpacked));
  EXPECT_EQ(addr, unpacked);

  EXPECT_EQ(kIPString, addr.ToString());
  EXPECT_EQ(kIPLiteral, IPAddressToURIString(addr));
  EXPECT_EQ(kPTRString, IPAddressToPTRString(addr));
  EXPECT_TRUE(PTRStringToIPAddress(kPTRString, &addr));
  EXPECT_EQ(kIPString, addr.ToString());
}

TEST(IPAddressTest, ToAndFromString6EightColons) {
  IPAddress addr;
  IPAddress expected;

  EXPECT_TRUE(StringToIPAddress("::7:6:5:4:3:2:1", &addr));
  EXPECT_TRUE(StringToIPAddress("0:7:6:5:4:3:2:1", &expected));
  EXPECT_EQ(expected, addr);

  EXPECT_TRUE(StringToIPAddress("7:6:5:4:3:2:1::", &addr));
  EXPECT_TRUE(StringToIPAddress("7:6:5:4:3:2:1:0", &expected));
  EXPECT_EQ(expected, addr);
}

TEST(IPAddressTest, EmptyStrings) {
  IPAddress ip;
  EXPECT_FALSE(StringToIPAddress(NULL, &ip));
  EXPECT_FALSE(StringToIPAddress("", &ip));
  std::string empty;
  EXPECT_FALSE(StringToIPAddress(empty, &ip));
}

TEST(IPAddressTest, Equality) {
  const std::string kIPv4String1 = "1.2.3.4";
  const std::string kIPv4String2 = "2.3.4.5";
  const std::string kIPv6String1 = "2001:700:300:1800::f";
  const std::string kIPv6String2 = "2001:700:300:1800:0:0:0:f";
  const std::string kIPv6String3 = "::1";

  IPAddress empty;
  IPAddress addr4_1, addr4_2;
  IPAddress addr6_1, addr6_2, addr6_3;

  ASSERT_TRUE(StringToIPAddress(kIPv4String1, &addr4_1));
  ASSERT_TRUE(StringToIPAddress(kIPv4String2, &addr4_2));
  ASSERT_TRUE(StringToIPAddress(kIPv6String1, &addr6_1));
  ASSERT_TRUE(StringToIPAddress(kIPv6String2, &addr6_2));
  ASSERT_TRUE(StringToIPAddress(kIPv6String3, &addr6_3));

  // operator==
  EXPECT_TRUE(empty == empty);
  EXPECT_FALSE(empty == addr4_1);
  EXPECT_FALSE(empty == addr4_2);
  EXPECT_FALSE(empty == addr6_1);
  EXPECT_FALSE(empty == addr6_2);
  EXPECT_FALSE(empty == addr6_3);

  EXPECT_FALSE(addr4_1 == empty);
  EXPECT_TRUE(addr4_1 == addr4_1);
  EXPECT_FALSE(addr4_1 == addr4_2);
  EXPECT_FALSE(addr4_1 == addr6_1);
  EXPECT_FALSE(addr4_1 == addr6_2);
  EXPECT_FALSE(addr4_1 == addr6_3);

  EXPECT_FALSE(addr4_2 == empty);
  EXPECT_FALSE(addr4_2 == addr4_1);
  EXPECT_TRUE(addr4_2 == addr4_2);
  EXPECT_FALSE(addr4_2 == addr6_1);
  EXPECT_FALSE(addr4_2 == addr6_2);
  EXPECT_FALSE(addr4_2 == addr6_3);

  EXPECT_FALSE(addr6_1 == empty);
  EXPECT_FALSE(addr6_1 == addr4_1);
  EXPECT_FALSE(addr6_1 == addr4_2);
  EXPECT_TRUE(addr6_1 == addr6_1);
  EXPECT_TRUE(addr6_1 == addr6_2);
  EXPECT_FALSE(addr6_1 == addr6_3);

  EXPECT_FALSE(addr6_2 == empty);
  EXPECT_FALSE(addr6_2 == addr4_1);
  EXPECT_FALSE(addr6_2 == addr4_2);
  EXPECT_TRUE(addr6_2 == addr6_1);
  EXPECT_TRUE(addr6_2 == addr6_2);
  EXPECT_FALSE(addr6_2 == addr6_3);

  EXPECT_FALSE(addr6_3 == empty);
  EXPECT_FALSE(addr6_3 == addr4_1);
  EXPECT_FALSE(addr6_3 == addr4_2);
  EXPECT_FALSE(addr6_3 == addr6_1);
  EXPECT_FALSE(addr6_3 == addr6_2);
  EXPECT_TRUE(addr6_3 == addr6_3);

  // operator!= (same tests, just inverted)
  EXPECT_FALSE(empty != empty);
  EXPECT_TRUE(empty != addr4_1);
  EXPECT_TRUE(empty != addr4_2);
  EXPECT_TRUE(empty != addr6_1);
  EXPECT_TRUE(empty != addr6_2);
  EXPECT_TRUE(empty != addr6_3);

  EXPECT_TRUE(addr4_1 != empty);
  EXPECT_FALSE(addr4_1 != addr4_1);
  EXPECT_TRUE(addr4_1 != addr4_2);
  EXPECT_TRUE(addr4_1 != addr6_1);
  EXPECT_TRUE(addr4_1 != addr6_2);
  EXPECT_TRUE(addr4_1 != addr6_3);

  EXPECT_TRUE(addr4_2 != empty);
  EXPECT_TRUE(addr4_2 != addr4_1);
  EXPECT_FALSE(addr4_2 != addr4_2);
  EXPECT_TRUE(addr4_2 != addr6_1);
  EXPECT_TRUE(addr4_2 != addr6_2);
  EXPECT_TRUE(addr4_2 != addr6_3);

  EXPECT_TRUE(addr6_1 != empty);
  EXPECT_TRUE(addr6_1 != addr4_1);
  EXPECT_TRUE(addr6_1 != addr4_2);
  EXPECT_FALSE(addr6_1 != addr6_1);
  EXPECT_FALSE(addr6_1 != addr6_2);
  EXPECT_TRUE(addr6_1 != addr6_3);

  EXPECT_TRUE(addr6_2 != empty);
  EXPECT_TRUE(addr6_2 != addr4_1);
  EXPECT_TRUE(addr6_2 != addr4_2);
  EXPECT_FALSE(addr6_2 != addr6_1);
  EXPECT_FALSE(addr6_2 != addr6_2);
  EXPECT_TRUE(addr6_2 != addr6_3);

  EXPECT_TRUE(addr6_3 != empty);
  EXPECT_TRUE(addr6_3 != addr4_1);
  EXPECT_TRUE(addr6_3 != addr4_2);
  EXPECT_TRUE(addr6_3 != addr6_1);
  EXPECT_TRUE(addr6_3 != addr6_2);
  EXPECT_FALSE(addr6_3 != addr6_3);
}

TEST(IPAddressTest, DISABLED_UInt32ToIPAddress) {
  // UInt32ToIPAddress is not allowed in depot3
  uint32 addr1 = htonl(0);
  uint32 addr2 = htonl(0x7f000001);
  uint32 addr3 = htonl(0xffffffff);

  EXPECT_EQ("0.0.0.0", UInt32ToIPAddress(addr1).ToString());
  EXPECT_EQ("127.0.0.1", UInt32ToIPAddress(addr2).ToString());
  EXPECT_EQ("255.255.255.255", UInt32ToIPAddress(addr3).ToString());
}

TEST(IPAddressTest, HostUInt32ToIPAddress) {
  uint32 addr1 = 0;
  uint32 addr2 = 0x7f000001;
  uint32 addr3 = 0xffffffff;

  EXPECT_EQ("0.0.0.0", HostUInt32ToIPAddress(addr1).ToString());
  EXPECT_EQ("127.0.0.1", HostUInt32ToIPAddress(addr2).ToString());
  EXPECT_EQ("255.255.255.255", HostUInt32ToIPAddress(addr3).ToString());
}

TEST(IPAddressTest, IPAddressToHostUInt32) {
  IPAddress addr = StringToIPAddressOrDie("1.2.3.4");
  EXPECT_EQ(0x01020304u, IPAddressToHostUInt32(addr));
}

TEST(IPAddressTest, UInt128ToIPAddress) {
  absl::uint128 addr1(0);
  absl::uint128 addr2(1);
  absl::uint128 addr3 = absl::MakeUint128(kuint64max, kuint64max);

  EXPECT_EQ("::", UInt128ToIPAddress(addr1).ToString());
  EXPECT_EQ("::1", UInt128ToIPAddress(addr2).ToString());
  EXPECT_EQ("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
            UInt128ToIPAddress(addr3).ToString());
}

TEST(IPAddressTest, Constants) {
  EXPECT_EQ("0.0.0.0", IPAddress::Any4().ToString());
  EXPECT_EQ("127.0.0.1", IPAddress::Loopback4().ToString());
  EXPECT_EQ("::", IPAddress::Any6().ToString());
  EXPECT_EQ("::1", IPAddress::Loopback6().ToString());

  EXPECT_TRUE(IsAnyIPAddress(IPAddress::Any4()));
  EXPECT_TRUE(IsAnyIPAddress(IPAddress::Any6()));
  EXPECT_TRUE(IsLoopbackIPAddress(IPAddress::Loopback4()));
  EXPECT_TRUE(IsLoopbackIPAddress(IPAddress::Loopback6()));
}

TEST(IPAddressTest, Loopback) {
  IPAddress ip;

  // Canonical loopback IP addresses.
  ip = IPAddress::Loopback4();
  EXPECT_TRUE(IsLoopbackIPAddress(ip));
  EXPECT_TRUE(IsCanonicalLoopbackIPAddress(ip));

  ip = IPAddress::Loopback6();
  EXPECT_TRUE(IsLoopbackIPAddress(ip));
  EXPECT_TRUE(IsCanonicalLoopbackIPAddress(ip));

  // Various addresses near or within 127.0.0.0/8.
  ip = StringToIPAddressOrDie("126.255.255.255");
  EXPECT_FALSE(IsLoopbackIPAddress(ip));
  EXPECT_FALSE(IsCanonicalLoopbackIPAddress(ip));

  ip = StringToIPAddressOrDie("127.0.0.0");
  EXPECT_TRUE(IsLoopbackIPAddress(ip));
  EXPECT_FALSE(IsCanonicalLoopbackIPAddress(ip));

  ip = StringToIPAddressOrDie("127.0.0.1");
  EXPECT_TRUE(IsLoopbackIPAddress(ip));
  EXPECT_TRUE(IsCanonicalLoopbackIPAddress(ip));

  ip = StringToIPAddressOrDie("127.1.2.3");
  EXPECT_TRUE(IsLoopbackIPAddress(ip));
  EXPECT_FALSE(IsCanonicalLoopbackIPAddress(ip));

  ip = StringToIPAddressOrDie("127.255.255.255");
  EXPECT_TRUE(IsLoopbackIPAddress(ip));
  EXPECT_FALSE(IsCanonicalLoopbackIPAddress(ip));

  ip = StringToIPAddressOrDie("128.0.0.0");
  EXPECT_FALSE(IsLoopbackIPAddress(ip));
  EXPECT_FALSE(IsCanonicalLoopbackIPAddress(ip));

  // Some random non-loopback addresses.
  ip = StringToIPAddressOrDie("10.0.0.1");
  EXPECT_FALSE(IsLoopbackIPAddress(ip));
  EXPECT_FALSE(IsCanonicalLoopbackIPAddress(ip));

  ip = StringToIPAddressOrDie("2001:700:300:1803:b0ff::12");
  EXPECT_FALSE(IsLoopbackIPAddress(ip));
  EXPECT_FALSE(IsCanonicalLoopbackIPAddress(ip));

  // 0.0.0.0 and ::.
  ip = IPAddress::Any4();
  EXPECT_FALSE(IsLoopbackIPAddress(ip));
  EXPECT_FALSE(IsCanonicalLoopbackIPAddress(ip));

  ip = IPAddress::Any6();
  EXPECT_FALSE(IsLoopbackIPAddress(ip));
  EXPECT_FALSE(IsCanonicalLoopbackIPAddress(ip));
}

TEST(IPAddressTest, Logging) {
  const std::string kIPv4String = "1.2.3.4";
  const std::string kIPv6String = "2001:700:300:1800::f";
  IPAddress addr4, addr6;

  ASSERT_TRUE(StringToIPAddress(kIPv4String, &addr4));
  ASSERT_TRUE(StringToIPAddress(kIPv6String, &addr6));

  std::ostringstream out;
  out << addr4 << " " << addr6;
  EXPECT_EQ("1.2.3.4 2001:700:300:1800::f", out.str());
}

TEST(IPAddressTest, LoggingUninitialized) {
  std::ostringstream out;
  out << IPAddress();
  EXPECT_EQ("<uninitialized IPAddress>", out.str());
}

#if HAVE_FIXEDARRAY
// No FixedArray
// Adapted from dnscache_unittest.cc.
namespace {

void TestChooseRandomAddress4(int N) {
  LOG(INFO) << "Test ChooseRandomAddress4() with " << N << " entries";

  // Make a fake host entry with N IP addresses.
  FixedArray<in_addr> ips(N + 1);
  FixedArray<char*> ptrs(N + 1);
  FixedArray<int> count(N);
  for (int i = 0; i < N; i++) {
    ptrs[i] = reinterpret_cast<char*>(&ips[i]);
    ips[i].s_addr = i;
    count[i] = 0;
  }
  struct hostent host;
  ptrs[N] = NULL;
  host.h_addrtype = AF_INET;
  host.h_addr_list = ptrs.get();

  // Ensure that if we do 100*N, we get at least each address once.
  for (int i = 0; i < N * 100; i++) {
    in_addr ip = ChooseRandomAddress(&host).ipv4_address();
    const int id = ip.s_addr;
    CHECK_GE(id, 0);
    CHECK_LT(id, N);
    CHECK_EQ(0, memcmp(&ips[id], &ip, sizeof(ip)));
    count[id]++;
  }

  for (int i = 0; i < N; i++) {
    CHECK_GT(count[i], 0);
  }
}

TEST(IPAddressTest, Joining) {
  std::vector<IPAddress> v = {
      StringToIPAddressOrDie("192.0.2.0"),
      StringToIPAddressOrDie("2001:db8::"),
      StringToIPAddressOrDie("0.0.0.0"),
      StringToIPAddressOrDie("::")
  };
  EXPECT_EQ("192.0.2.0!!!2001:db8::!!!0.0.0.0!!!::",
            std::strings::Join(v, "!!!", IPAddressJoinFormatter()));
}

void TestChooseRandomAddress6(int N) {
  LOG(INFO) << "Test ChooseRandomAddress6() with " << N << " entries";

  // Make a fake host entry with N IP addresses.
  FixedArray<in6_addr> ips(N + 1);
  FixedArray<char*> ptrs(N + 1);
  FixedArray<int> count(N);
  for (int i = 0; i < N; i++) {
    ptrs[i] = reinterpret_cast<char*>(&ips[i]);
    ips[i].s6_addr32[0] = i;
    ips[i].s6_addr32[1] = i*2;
    ips[i].s6_addr32[2] = i*3;
    ips[i].s6_addr32[3] = i*4;
    count[i] = 0;
  }
  struct hostent host;
  ptrs[N] = NULL;
  host.h_addrtype = AF_INET6;
  host.h_addr_list = ptrs.get();

  // Ensure that if we do 100*N, we get at least each address once.
  for (int i = 0; i < N * 100; i++) {
    in6_addr ip = ChooseRandomAddress(&host).ipv6_address();
    const int id = ip.s6_addr32[0];
    CHECK_GE(id, 0);
    CHECK_LT(id, N);
    CHECK_EQ(0, memcmp(&ips[id], &ip, sizeof(ip)));
    count[id]++;
  }

  for (int i = 0; i < N; i++) {
    CHECK_GT(count[i], 0);
  }
}

void TestChooseRandomIPAddress(int N) {
  LOG(INFO) << "Test ChooseRandomIPAddress() with " << N << " entries";

  FixedArray<int> count(N);
  std::vector<IPAddress> ipvec;
  ipvec.reserve(N);
  for (int i = 0; i < N; i++) {
    in6_addr ip6;
    ip6.s6_addr32[0] = i;
    ip6.s6_addr32[1] = i*2;
    ip6.s6_addr32[2] = i*3;
    ip6.s6_addr32[3] = i*4;

    ipvec.push_back(IPAddress(ip6));
    count[i] = 0;
  }

  // Ensure that if we do 100*N, we get at least each address once.
  for (int i = 0; i < N * 100; i++) {
    IPAddress ip = ChooseRandomIPAddress(&ipvec);
    const int id = ip.ipv6_address().s6_addr32[0];
    CHECK_GE(id, 0);
    CHECK_LT(id, N);
    CHECK_EQ(ip, ipvec[id]);
    count[id]++;
  }

  for (int i = 0; i < N; i++) {
    CHECK_GT(count[i], 0);
  }
}

}  // namespace

TEST(IPAddressTest, ChooseRandomAddress) {
  TestChooseRandomAddress4(1);
  TestChooseRandomAddress4(2);
  TestChooseRandomAddress4(10);
  TestChooseRandomAddress4(40);

  TestChooseRandomAddress6(1);
  TestChooseRandomAddress6(2);
  TestChooseRandomAddress6(10);
  TestChooseRandomAddress6(40);

  TestChooseRandomIPAddress(1);
  TestChooseRandomIPAddress(2);
  TestChooseRandomIPAddress(10);
  TestChooseRandomIPAddress(40);
}
#endif

TEST(IPAddressTest, IPAddressOrdering) {
  const std::string kIPv4String1 = "1.2.3.4";
  const std::string kIPv4String2 = "4.3.2.1";
  const std::string kIPv6String1 = "2001:700:300:1800::f";
  const std::string kIPv6String2 = "2001:700:300:1800:0:0:0:f";
  const std::string kIPv6String3 = "::1";
  const std::string kIPv6String4 = "::4";

  IPAddress addr0;  // uninitialized
  IPAddress addr4_1, addr4_2;
  IPAddress addr6_1, addr6_2, addr6_3, addr6_4;

  ASSERT_TRUE(StringToIPAddress(kIPv4String1, &addr4_1));
  ASSERT_TRUE(StringToIPAddress(kIPv4String2, &addr4_2));
  ASSERT_TRUE(StringToIPAddress(kIPv6String1, &addr6_1));
  ASSERT_TRUE(StringToIPAddress(kIPv6String2, &addr6_2));
  ASSERT_TRUE(StringToIPAddress(kIPv6String3, &addr6_3));
  ASSERT_TRUE(StringToIPAddress(kIPv6String4, &addr6_4));

  std::set<IPAddress, IPAddressOrdering> addrs;
  addrs.insert(addr6_2);
  addrs.insert(addr4_2);
  addrs.insert(addr6_1);
  addrs.insert(addr4_1);
  addrs.insert(addr0);
  addrs.insert(addr6_3);
  addrs.insert(addr6_4);

  EXPECT_EQ(6u, addrs.size());

  std::vector<IPAddress> sorted_addrs(addrs.begin(), addrs.end());
  ASSERT_EQ(6u, sorted_addrs.size());
  EXPECT_EQ(addr0, sorted_addrs[0]);
  EXPECT_EQ(addr4_1, sorted_addrs[1]);
  EXPECT_EQ(addr4_2, sorted_addrs[2]);
  EXPECT_EQ(addr6_3, sorted_addrs[3]);
  EXPECT_EQ(addr6_4, sorted_addrs[4]);
  EXPECT_EQ(addr6_1, sorted_addrs[5]);
}

TEST(IPAddressTest, Hash) {
  const std::string kIPv4String1 = "1.2.3.4";
  const std::string kIPv4String2 = "4.3.2.1";
  const std::string kIPv6String1 = "2001:700:300:1800::f";
  const std::string kIPv6String2 = "2001:700:300:1800:0:0:0:f";
  const std::string kIPv6String3 = "::1";
  const std::string kIPv6String4 = "::4";

  IPAddress addr0;
  IPAddress addr4_1, addr4_2;
  IPAddress addr6_1, addr6_2, addr6_3, addr6_4;

  ASSERT_TRUE(StringToIPAddress(kIPv4String1, &addr4_1));
  ASSERT_TRUE(StringToIPAddress(kIPv4String2, &addr4_2));
  ASSERT_TRUE(StringToIPAddress(kIPv6String1, &addr6_1));
  ASSERT_TRUE(StringToIPAddress(kIPv6String2, &addr6_2));
  ASSERT_TRUE(StringToIPAddress(kIPv6String3, &addr6_3));
  ASSERT_TRUE(StringToIPAddress(kIPv6String4, &addr6_4));

  hash_set<IPAddress> addrs;
  addrs.insert(addr0);
  addrs.insert(IPAddress());
  addrs.insert(addr6_2);
  addrs.insert(addr4_2);
  addrs.insert(addr6_1);
  addrs.insert(addr4_1);
  addrs.insert(addr6_3);
  addrs.insert(addr6_4);

  EXPECT_EQ(6u, addrs.size());

  EXPECT_EQ(1u, addrs.count(addr0));
  EXPECT_EQ(1u, addrs.count(addr4_1));
  EXPECT_EQ(1u, addrs.count(addr4_2));
  EXPECT_EQ(1u, addrs.count(addr6_1));
  EXPECT_EQ(1u, addrs.count(addr6_2));
  EXPECT_EQ(1u, addrs.count(addr6_3));
  EXPECT_EQ(1u, addrs.count(addr6_4));
}

TEST(IPAddressTest, v6Mapped) {
  const std::string kIPv4String = "1.2.3.4";
  const std::string kCompatibleIPString = "::1.2.3.4";
  const std::string kMappedIPString = "::ffff:1.2.3.4";
  IPAddress addr4, compatible_addr, mapped_addr;

  ASSERT_TRUE(StringToIPAddress(kIPv4String, &addr4));
  ASSERT_TRUE(StringToIPAddress(kCompatibleIPString, &compatible_addr));
  ASSERT_TRUE(StringToIPAddress(kMappedIPString, &mapped_addr));
  EXPECT_EQ(kMappedIPString, mapped_addr.ToString());
  EXPECT_EQ(kCompatibleIPString, compatible_addr.ToString());

  // We've specified explicitly that these should be distinct --
  // one might agree or disagree with the decision, but as long as
  // it stands, we should test the behavior.
  EXPECT_FALSE(addr4 == mapped_addr);
  EXPECT_TRUE(addr4 != mapped_addr);

  IPAddress compare4 = IPAddress::Any4();
  EXPECT_FALSE(GetCompatIPv4Address(mapped_addr, NULL));
  EXPECT_TRUE(GetMappedIPv4Address(mapped_addr, NULL));
  EXPECT_TRUE(GetMappedIPv4Address(mapped_addr, &compare4));
  EXPECT_TRUE(addr4 == compare4);

  EXPECT_FALSE(addr4 == compatible_addr);
  EXPECT_TRUE(addr4 != compatible_addr);

  compare4 = IPAddress::Any4();
  EXPECT_FALSE(GetMappedIPv4Address(compatible_addr, NULL));
  EXPECT_TRUE(GetCompatIPv4Address(compatible_addr, NULL));
  EXPECT_TRUE(GetCompatIPv4Address(compatible_addr, &compare4));
  EXPECT_TRUE(addr4 == compare4);

  EXPECT_FALSE(mapped_addr == compatible_addr);
  EXPECT_TRUE(mapped_addr != compatible_addr);

  // Test ordering.
  IPAddressOrdering order;
  EXPECT_TRUE(order(addr4, mapped_addr));
  EXPECT_FALSE(order(mapped_addr, addr4));

  EXPECT_TRUE(order(addr4, compatible_addr));
  EXPECT_FALSE(order(compatible_addr, addr4));

  // Test hashing.
  hash_set<IPAddress> addrs;
  addrs.insert(addr4);
  addrs.insert(mapped_addr);
  addrs.insert(compatible_addr);
  EXPECT_EQ(3u, addrs.size());
}

// Test case shamelessly lifted from:
//
//     http://en.wikipedia.org/wiki/6to4#Address_block_allocation
//
// """
// Thus for the global IPv4 address 207.142.131.202, the corresponding
// 6to4 prefix would be 2002:CF8E:83CA::/48.
// """
TEST(IPAddressTest, Get6to4IPv4Address) {
  const IPAddress addr4 = StringToIPAddressOrDie("207.142.131.202");
  const IPAddress addr6 = StringToIPAddressOrDie("2002:cf8e:83ca::");
  IPAddress compare4;

  EXPECT_FALSE(Get6to4IPv4Address(addr4, NULL));
  EXPECT_TRUE(Get6to4IPv4Address(addr6, NULL));
  EXPECT_TRUE(Get6to4IPv4Address(addr6, &compare4));
  EXPECT_EQ(addr4, compare4);
}

TEST(IPAddressTest, Get6to4IPv6Range) {
  IPRange iprange6;

  const IPAddress addr4 = StringToIPAddressOrDie("207.142.131.202");
  const IPAddress addr6 = StringToIPAddressOrDie("2002:cf8e:83ca::");

  EXPECT_FALSE(Get6to4IPv6Range(IPRange(addr6), NULL));
  EXPECT_FALSE(Get6to4IPv6Range(IPRange::Any6(), NULL));

  EXPECT_TRUE(Get6to4IPv6Range(IPRange::Any4(), &iprange6));
  EXPECT_EQ(StringToIPRangeOrDie("2002::/16"), iprange6);

  EXPECT_TRUE(Get6to4IPv6Range(IPRange(addr4), NULL));
  EXPECT_TRUE(Get6to4IPv6Range(IPRange(addr4), &iprange6));
  EXPECT_EQ(StringToIPRangeOrDie("2002:cf8e:83ca::/48"), iprange6);

  for (int len4 = 0; len4 <= 32; len4++) {
    const int len6 = len4 + 16;
    EXPECT_TRUE(Get6to4IPv6Range(IPRange(addr4, len4), &iprange6));
    EXPECT_EQ(IPRange(addr6, len6), iprange6);
    EXPECT_EQ(TruncateIPAddress(addr6, len6),
              NthAddressInRange(iprange6, 0));
    // Make sure reverse direction also works.
    IPAddress compare4;
    EXPECT_TRUE(Get6to4IPv4Address(NthAddressInRange(iprange6, 0), &compare4));
    EXPECT_EQ(TruncateIPAddress(addr4, len4), compare4);
  }
}

TEST(IPAddressTest, GetIsatapIPv4Address) {
  const std::string kIPv4Address = "207.142.131.202";
  const std::string kBadIsatapAddress = "2001:db8::0040:5efe:cf8e:83ca";
  const std::string kTeredoAddress = "2001:0:102:203:200:5efe:506:708";
  const char* kIsatapAddresses[] = {
    "2001:db8::5efe:cf8e:83ca",
    "2001:db8::100:5efe:cf8e:83ca",  // Private Multicast? Not likely.
    "2001:db8::200:5efe:cf8e:83ca",
    "2001:db8::300:5efe:cf8e:83ca"   // Public Multicast? Also unlikely.
  };
  IPAddress addr4, addr6, compare4;

  ASSERT_TRUE(StringToIPAddress(kIPv4Address, &addr4));
  EXPECT_FALSE(GetIsatapIPv4Address(addr4, NULL));

  ASSERT_TRUE(StringToIPAddress(kBadIsatapAddress, &addr6));
  EXPECT_FALSE(GetIsatapIPv4Address(addr6, NULL));

  ASSERT_TRUE(StringToIPAddress(kTeredoAddress, &addr6));
  EXPECT_TRUE(GetTeredoInfo(addr6, NULL, NULL, NULL, NULL));
  EXPECT_FALSE(GetIsatapIPv4Address(addr6, NULL));

  for (size_t i = 0; i < ARRAYSIZE(kIsatapAddresses); i++) {
    ASSERT_TRUE(StringToIPAddress(kIsatapAddresses[i], &addr6));
    EXPECT_TRUE(GetIsatapIPv4Address(addr6, NULL));
    EXPECT_TRUE(GetIsatapIPv4Address(addr6, &compare4));
    EXPECT_TRUE(addr4 == compare4);
  }
}

// Shamelessly lifted from:
//
//     http://en.wikipedia.org/wiki/Teredo_tunneling#Teredo_IPv6_addressing
//
// """
// As an example, 2001:0000:4136:e378:8000:63bf:3fff:fdd2 refers to a
// Teredo client:
//
//     * using Teredo server at address 65.54.227.120
//       (4136e378 in hexadecimal),
//     * located behind a cone NAT (bit 64 is set),
//     * using UDP mapped port 40000 on its NAT
//       (in hexadecimal 63bf xor ffff equals 9c40, or decimal number 40000),
//     * whose NAT has public IPv4 address 192.0.2.45
//       (3ffffdd2 xor ffffffff equals c000022d, which is to say 192.0.2.45).
// """
TEST(IPAddressTest, GetTeredoInfo) {
  const std::string kTeredoAddress = "2001:0000:4136:e378:8000:63bf:3fff:fdd2";
  const std::string kTeredoServer = "65.54.227.120";
  const uint16 kFlags = 0x8000;
  const uint16 kPort = 40000;
  const std::string kTeredoClient = "192.0.2.45";

  IPAddress addr4c, addr4s, addr6, client, server;
  uint16 flags, port;

  ASSERT_TRUE(StringToIPAddress(kTeredoAddress, &addr6));
  ASSERT_TRUE(StringToIPAddress(kTeredoClient, &addr4c));
  ASSERT_TRUE(StringToIPAddress(kTeredoServer, &addr4s));
  EXPECT_FALSE(GetTeredoInfo(addr4c, NULL, NULL, NULL, NULL));
  EXPECT_TRUE(GetTeredoInfo(addr6, NULL, NULL, NULL, NULL));
  EXPECT_TRUE(GetTeredoInfo(addr6, &server, &flags, &port, &client));
  EXPECT_TRUE(addr4s == server);
  EXPECT_EQ(kFlags, flags);
  EXPECT_EQ(kPort, port);
  EXPECT_TRUE(addr4c == client);
}

TEST(IPAddressTest, GetEmbeddedIPv4ClientAddress) {
  const std::string kIPv4String = "1.2.3.4";
  const std::string kCompatibleIPString = "::1.2.3.4";
  const std::string kMappedIPString = "::ffff:1.2.3.4";
  const std::string kTeredoClient = "192.0.2.45";
  const std::string kTeredoAddress = "2001:0000:4136:e378:8000:63bf:3fff:fdd2";
  const std::string kIPv4Address = "207.142.131.202";
  const std::string k6to4Address = "2002:cf8e:83ca::";
  const std::string kIsatapAddress = "2001:db8::200:5efe:cf8e:83ca";

  IPAddress ip4, ip6, embedded;

  // IPv4 address.
  ASSERT_TRUE(StringToIPAddress(kIPv4String, &ip4));
  EXPECT_FALSE(GetEmbeddedIPv4ClientAddress(ip4, NULL));

  // Compatible IPv4 address.
  ASSERT_TRUE(StringToIPAddress(kCompatibleIPString, &ip6));
  EXPECT_TRUE(GetEmbeddedIPv4ClientAddress(ip6, &embedded));
  EXPECT_EQ(ip4, embedded);

  // Mapped IPv6 address.
  ASSERT_TRUE(StringToIPAddress(kMappedIPString, &ip6));
  EXPECT_TRUE(GetEmbeddedIPv4ClientAddress(ip6, &embedded));
  EXPECT_EQ(ip4, embedded);

  // Teredo.
  ASSERT_TRUE(StringToIPAddress(kTeredoClient, &ip4));
  ASSERT_TRUE(StringToIPAddress(kTeredoAddress, &ip6));
  EXPECT_TRUE(GetEmbeddedIPv4ClientAddress(ip6, &embedded));
  EXPECT_EQ(ip4, embedded);

  // 6to4.
  ASSERT_TRUE(StringToIPAddress(kIPv4Address, &ip4));
  ASSERT_TRUE(StringToIPAddress(k6to4Address, &ip6));
  EXPECT_TRUE(GetEmbeddedIPv4ClientAddress(ip6, &embedded));
  EXPECT_EQ(ip4, embedded);

  // ISATAP: Assert that ISATAP addresses, so easily spoofable,
  // do not find their way into this method by some future chance.
  ASSERT_TRUE(StringToIPAddress(kIPv4Address, &ip4));
  ASSERT_TRUE(StringToIPAddress(kIsatapAddress, &ip6));
  EXPECT_FALSE(GetEmbeddedIPv4ClientAddress(ip6, &embedded));
}

TEST(IPAddressTest, DISABLED_GetCoercedIPv4Address_Special) {
  const char* kIPv4String = "1.2.3.4";
  const char* kCompatibleIPString = "::1.2.3.4";
  const char* kMappedIPString = "::ffff:1.2.3.4";
  const char* kTeredoClient = "192.0.2.45";
  const char* kTeredoAddress = "2001:0000:4136:e378:8000:63bf:3fff:fdd2";
  const char* kIPv4Address = "207.142.131.202";
  const char* k6to4Address = "2002:cf8e:83ca::";
  const char* kLocalhost6Address = "::1";
  const char* kLocalhost4Address = "127.0.0.1";
  const char* kAny6Address = "::";
  const char* kAny4Address = "0.0.0.0";

  IPAddress ip4, ip6, coerced;

  // IPv4 address.
  ASSERT_TRUE(StringToIPAddress(kIPv4String, &ip4));
  coerced = GetCoercedIPv4Address(ip4);
  EXPECT_EQ(ip4, coerced);

  // Compatible IPv4 address.
  ASSERT_TRUE(StringToIPAddress(kCompatibleIPString, &ip6));
  coerced = GetCoercedIPv4Address(ip6);
  EXPECT_NE(ip4, coerced);

  // Mapped IPv6 address.
  ASSERT_TRUE(StringToIPAddress(kMappedIPString, &ip6));
  coerced = GetCoercedIPv4Address(ip6);
  EXPECT_NE(ip4, coerced);

  // Teredo.
  ASSERT_TRUE(StringToIPAddress(kTeredoClient, &ip4));
  ASSERT_TRUE(StringToIPAddress(kTeredoAddress, &ip6));
  coerced = GetCoercedIPv4Address(ip6);
  EXPECT_NE(ip4, coerced);

  // 6to4.
  ASSERT_TRUE(StringToIPAddress(kIPv4Address, &ip4));
  ASSERT_TRUE(StringToIPAddress(k6to4Address, &ip6));
  coerced = GetCoercedIPv4Address(ip6);
  EXPECT_NE(ip4, coerced);

  // Localhost (special case).
  ASSERT_TRUE(StringToIPAddress(kLocalhost4Address, &ip4));
  ASSERT_TRUE(StringToIPAddress(kLocalhost6Address, &ip6));
  coerced = GetCoercedIPv4Address(ip6);
  EXPECT_EQ(ip4, coerced);

  // Any address (special case).
  ASSERT_TRUE(StringToIPAddress(kAny4Address, &ip4));
  ASSERT_TRUE(StringToIPAddress(kAny6Address, &ip6));
  coerced = GetCoercedIPv4Address(ip6);
  EXPECT_EQ(ip4, coerced);
}

TEST(IPAddressTest, DISABLED_GetCoercedIPv4Address_Hashed_GeneralProperties) {
  const int kMaxIterations = 300;
  IPAddress ip6, coerced;
  in6_addr addr6;

  std::default_random_engine generator;
  std::uniform_int_distribution<uint32> uniform;

  for (int i = 0; i < kMaxIterations; i++) {
    addr6.s6_addr32[0] = uniform(generator);
    addr6.s6_addr32[1] = uniform(generator);
    addr6.s6_addr32[2] = uniform(generator);
    addr6.s6_addr32[3] = uniform(generator);

    // Make sure the address doesn't randomly end up being any kind of
    // address that would return a "fixed" IPv4 address, i.e. make sure
    // it's not 6to4, Teredo, etc.  So just pretend it's a 6bone (v2)
    // address.  See http://tools.ietf.org/html/rfc3701 for 6bone phaseout.
    addr6.s6_addr16[0] = htons(0x3ffe);

    ip6 = IPAddress(addr6);
    coerced = GetCoercedIPv4Address(ip6);

    SCOPED_TRACE(absl::Substitute("iter[$0]: ip6 '$1', coerced '$2'", i,
                                  ip6.ToString().c_str(),
                                  coerced.ToString().c_str()));

    // Make sure it's in the multicast + 240reserved space.
    uint32 high_byte = ((ntohl(coerced.ipv4_address().s_addr) >> 24)
                        & 0x000000ff);
    EXPECT_GE(high_byte, 224u);

    // Make sure it's not all 1's.
    EXPECT_NE(coerced.ipv4_address().s_addr, 0xffffffff);

    // Make sure it's repeatable.
    EXPECT_EQ(coerced, GetCoercedIPv4Address(ip6));
  }
}

// Although the mapping is arbitrary, we want consistent IPv6 -> IPv4 hashing
// over time and over platforms. Thus, this test makes a basic sanity check for
// a specific address.
TEST(IPAddressTest, DISABLED_GetCoercedIPv4Address_Hashed_SpecificExample) {
  IPAddress addr, coerced;

  ASSERT_TRUE(StringToIPAddress("2001:4860::1", &addr));
  ASSERT_TRUE(StringToIPAddress("242.163.117.221", &coerced));

  EXPECT_EQ(coerced, GetCoercedIPv4Address(addr));
}

TEST(IPAddressTest, NormalizeIPAddress) {
  IPAddress addr4, mapped_addr, compat_addr;

  ASSERT_TRUE(StringToIPAddress("129.241.93.35", &addr4));
  ASSERT_TRUE(StringToIPAddress("::ffff:129.241.93.35", &mapped_addr));
  ASSERT_TRUE(StringToIPAddress("::129.241.93.35", &compat_addr));

  EXPECT_EQ(addr4, NormalizeIPAddress(addr4));
  EXPECT_EQ(addr4, NormalizeIPAddress(mapped_addr));
  EXPECT_EQ(compat_addr, NormalizeIPAddress(compat_addr));

  IPAddress addr6;
  ASSERT_TRUE(StringToIPAddress("2001:700:300:1803::1", &addr6));
  EXPECT_EQ(addr6, NormalizeIPAddress(addr6));
  EXPECT_EQ(IPAddress::Loopback6(), NormalizeIPAddress(IPAddress::Loopback6()));
  EXPECT_EQ(IPAddress::Any6(), NormalizeIPAddress(IPAddress::Any6()));

  EXPECT_EQ(IPAddress(), NormalizeIPAddress(IPAddress()));
}

TEST(IPAddressTest, DualstackIPAddress) {
  IPAddress addr4 = StringToIPAddressOrDie("192.0.2.1");
  IPAddress mapped_addr = StringToIPAddressOrDie("::ffff:192.0.2.1");
  IPAddress compat_addr = StringToIPAddressOrDie("::192.0.2.1");

  EXPECT_EQ(mapped_addr, DualstackIPAddress(addr4));
  EXPECT_EQ(mapped_addr, DualstackIPAddress(mapped_addr));
  EXPECT_EQ(compat_addr, DualstackIPAddress(compat_addr));

  EXPECT_EQ(StringToIPAddressOrDie("::ffff:127.0.0.1"),
            DualstackIPAddress(IPAddress::Loopback4()));
  EXPECT_EQ(StringToIPAddressOrDie("::ffff:0.0.0.0"),
            DualstackIPAddress(IPAddress::Any4()));

  IPAddress addr6;
  ASSERT_TRUE(StringToIPAddress("2001:db8::1", &addr6));
  EXPECT_EQ(addr6, DualstackIPAddress(addr6));
  EXPECT_EQ(IPAddress::Loopback6(), DualstackIPAddress(IPAddress::Loopback6()));
  EXPECT_EQ(IPAddress::Any6(), DualstackIPAddress(IPAddress::Any6()));
}

TEST(IPAddressTest, IsInitializedAddress) {
  IPAddress uninit_addr, addr4, addr6;

  EXPECT_FALSE(IsInitializedAddress(uninit_addr));
  EXPECT_FALSE(IsInitializedAddress(addr4));
  EXPECT_FALSE(IsInitializedAddress(addr6));

  ASSERT_TRUE(StringToIPAddress("129.241.93.35", &addr4));
  ASSERT_TRUE(StringToIPAddress("2001:700:300:1803::1", &addr6));

  EXPECT_FALSE(IsInitializedAddress(uninit_addr));
  EXPECT_TRUE(IsInitializedAddress(addr4));
  EXPECT_TRUE(IsInitializedAddress(addr6));
}

TEST(IPAddressTest, IPAddressLength) {
  IPAddress ip;
  ASSERT_TRUE(StringToIPAddress("1.2.3.4", &ip));
  EXPECT_EQ(32, IPAddressLength(ip));
  ASSERT_TRUE(StringToIPAddress("2001:db8::1", &ip));
  EXPECT_EQ(128, IPAddressLength(ip));
}

TEST(IPAddressTest, PTRStringToIPAddress) {
  // Test malformed addresses only, valid addresses are tested for v4/v6 in the
  // corresponding v4/v6 conversion methods above.
  IPAddress ip;
  EXPECT_FALSE(PTRStringToIPAddress("1.0.127.in-addr.arpa", &ip));
  EXPECT_FALSE(PTRStringToIPAddress("1..0.127.in-addr.arpa", &ip));
  EXPECT_FALSE(PTRStringToIPAddress("1.0.0.256.in-addr.arpa", &ip));
  EXPECT_FALSE(PTRStringToIPAddress("1.0.-1.127.in-addr.arpa", &ip));
  EXPECT_FALSE(PTRStringToIPAddress("1.0.1a.127.in-addr.arpa", &ip));
  EXPECT_FALSE(PTRStringToIPAddress(" 1.0.0.127.in-addr.arpa", &ip));
  EXPECT_FALSE(PTRStringToIPAddress("+1.0.0.127.in-addr.arpa", &ip));
  EXPECT_FALSE(PTRStringToIPAddress("1.0.0.127.ip6.arpa", &ip));
  EXPECT_FALSE(PTRStringToIPAddress("1.1.0.1.0.0.0.0.0.0.0.0.0.0.0.0.3.0.8.0."
                                    "1.0.0.4.0.6.8.4.1.0.0.ip6.arpa.", &ip));
  EXPECT_FALSE(PTRStringToIPAddress("1..0.1.0.0.0.0.0.0.0.0.0.0.0.0.3.0.8.0."
                                    "1.0.0.4.0.6.8.4.1.0.0.2.ip6.arpa", &ip));
  EXPECT_FALSE(PTRStringToIPAddress("1.10.0.1.0.0.0.0.0.0.0.0.0.0.0.0.3.0.8.0."
                                    "1.0.0.4.0.6.8.4.1.0.0.2.ip6.arpa", &ip));
  EXPECT_FALSE(PTRStringToIPAddress("1.0.0.1.0.0.0.0.0.0.0.0.0.0.0.0.3.0.8.0."
                                    "1.0.0.4.0.6.8.4.1...0.2.ip6.arpa", &ip));
  EXPECT_FALSE(PTRStringToIPAddress("1.G.0.1.0.0.0.0.0.0.0.0.0.0.0.0.3.0.8.0."
                                    "1.0.0.4.0.6.8.4.1.0.0.2.ip6.arpa", &ip));
  EXPECT_FALSE(PTRStringToIPAddress("1.g.0.1.0.0.0.0.0.0.0.0.0.0.0.0.3.0.8.0."
                                    "1.0.0.4.0.6.8.4.1.0.0.2.ip6.arpa", &ip));
}

TEST(IPAddressDeathTest, IPAddressLength) {
  IPAddress ip;
  int bitlength = 0;

  ASSERT_FALSE(IsInitializedAddress(ip));

  EXPECT_DEATH(bitlength = IPAddressLength(ip), "");
}

TEST(IPAddressTest, IPAddressToUInt128) {
  IPAddress addr;
  ASSERT_TRUE(StringToIPAddress("2001:700:300:1803:b0ff::12", &addr));
  EXPECT_EQ(absl::MakeUint128(0x2001070003001803ULL, 0xb0ff000000000012ULL),
            IPAddressToUInt128(addr));
}

// Various death tests for IPAddress emergency behavior in production that
// should simply result in CHECK failures in debug mode.

TEST(IPAddressDeathTest, EmergencyCoercion) {
  const std::string kIPv6Address = "2001:700:300:1803::1";
  IPAddress addr;
  in_addr addr4;

  CHECK(StringToIPAddress(kIPv6Address, &addr));

  EXPECT_DEATH(addr4 = addr.ipv4_address(), "Check failed");
}

TEST(IPAddressDeathTest, EmergencyCompatibility) {
  const std::string kIPv4Address = "129.240.2.40";
  IPAddress addr;
  in6_addr addr6;

  CHECK(StringToIPAddress(kIPv4Address, &addr));

  EXPECT_DEATH(addr6 = addr.ipv6_address(), "Check failed");
}

TEST(IPAddressDeathTest, EmergencyEmptyString) {
  IPAddress empty;

  EXPECT_DEATH(empty.ToString(), "empty IPAddress");
}

TEST(IPAddressDeathTest, EmergencyEmptyURIString) {
  IPAddress empty;

  EXPECT_DEATH(IPAddressToURIString(empty), "empty IPAddress");
}

TEST(IPAddressDeathTest, EmergencyEmptyPTRString) {
  IPAddress empty;

  EXPECT_DEATH(IPAddressToPTRString(empty), "empty IPAddress");
}

TEST(IPAddressDeathTest, EmergencyIsNotAnyOrLoopback) {
  IPAddress empty;

  EXPECT_DEATH(IsAnyIPAddress(empty), "empty IPAddress");
  EXPECT_DEATH(IsLoopbackIPAddress(empty), "empty IPAddress");
}

// Invalid conversion in *OrDie() functions.
TEST(IPAddressDeathTest, InvalidStringConversion) {
  // Invalid conversion.
  EXPECT_DEATH(StringToIPAddressOrDie("foo"), "Invalid IP foo");
  EXPECT_DEATH(StringToIPAddressOrDie(std::string("172.1.1.300")),
               "Invalid IP");
  EXPECT_DEATH(StringToIPAddressOrDie("::g"), "Invalid IP");

  // Valid conversion.
  EXPECT_EQ(StringToIPAddressOrDie("1.2.3.4").ToString(), "1.2.3.4");
  EXPECT_EQ(StringToIPAddressOrDie("2001:700:300:1803::1").ToString(),
            "2001:700:300:1803::1");
}

TEST(IPAddressDeathTest, InvalidPackedStringConversion) {
  // Invalid conversion.
  EXPECT_DEATH(PackedStringToIPAddressOrDie("foo", 3), "Invalid packed IP");
  EXPECT_DEATH(PackedStringToIPAddressOrDie("bar"), "Invalid packed IP");

  // Valid conversion.
  const std::string packed = StringToIPAddressOrDie("1.2.3.4").ToPackedString();
  EXPECT_EQ(PackedStringToIPAddressOrDie(packed).ToString(), "1.2.3.4");
}

TEST(ColonlessHexToIPv6AddressTest, BogusInput) {
  const char* bogus[] = {
    // NULL,
    "",
    "bogus",
    "deadbeef",
    "fe80000000000000000573fffea000650",  // too long by one characer
    "fe80000000000000000573fffea0006",  // too short by one characer
    "fe80000000000000000573fffea0006x",  // not all hex
    "+e80000000000000000573fffea00065",  // not all hex
    "0x80000000000000000573fffea00065",  // not all hex
  };

  IPAddress dummy;
  for (size_t i = 0; i < ARRAYSIZE(bogus); ++i) {
    EXPECT_FALSE(ColonlessHexToIPv6Address(bogus[i], NULL));
    EXPECT_FALSE(ColonlessHexToIPv6Address(bogus[i], &dummy));
  }
}

TEST(ColonlessHexToIPv6AddressTest, BasicValidity) {
  const char* hex_str = "fe80000000000000000573fFfEa00065";
  const char* ip6_str = "fe80::5:73ff:fea0:65";
  IPAddress expected, parsed;

  ASSERT_TRUE(StringToIPAddress(ip6_str, &expected));
  EXPECT_TRUE(ColonlessHexToIPv6Address(hex_str, NULL));
  EXPECT_TRUE(ColonlessHexToIPv6Address(hex_str, &parsed));
  EXPECT_EQ(expected, parsed);
}

#if 0
// Some extra reinterpret_cast restrictions in depot3
// Tests for SocketAddress.
TEST(SocketAddressTest, BasicTest4) {
  const uint16 kPort = 64738;
  const uint16 kNetworkByteOrderPort = htons(kPort);

  IPAddress addr4;
  ASSERT_TRUE(StringToIPAddress("1.2.3.4", &addr4));
  SocketAddress sockaddr(addr4, kPort);

  EXPECT_EQ(addr4, sockaddr.host());
  EXPECT_EQ(kPort, sockaddr.port());

  sockaddr_in returned_addr4 = sockaddr.ipv4_address();
  EXPECT_EQ(AF_INET, returned_addr4.sin_family);
  EXPECT_EQ(addr4, IPAddress(returned_addr4.sin_addr));
  EXPECT_EQ(kNetworkByteOrderPort, returned_addr4.sin_port);

  std::string packed = sockaddr.ToPackedString();
  EXPECT_EQ(sizeof(returned_addr4.sin_addr) + 2, packed.length());
  EXPECT_EQ(0, memcmp(packed.data(), &returned_addr4.sin_addr,
                      sizeof(returned_addr4.sin_addr)));
  EXPECT_EQ(0, memcmp(packed.data() + sizeof(returned_addr4.sin_addr),
                      &kNetworkByteOrderPort, sizeof(kNetworkByteOrderPort)));

  EXPECT_TRUE(PackedStringToSocketAddress(packed, nullptr));
  SocketAddress unpacked;
  EXPECT_TRUE(PackedStringToSocketAddress(packed, &unpacked));
  EXPECT_EQ(sockaddr, unpacked);

  sockaddr_storage returned_addr_generic = sockaddr.generic_address();
  EXPECT_EQ(AF_INET, returned_addr_generic.ss_family);
  sockaddr_in* cast_addr4
      = reinterpret_cast<sockaddr_in*>(&returned_addr_generic);

  EXPECT_EQ(addr4, IPAddress(cast_addr4->sin_addr));
  EXPECT_EQ(kNetworkByteOrderPort, cast_addr4->sin_port);

  // Test copy construction.
  SocketAddress another_sockaddr = sockaddr;
  EXPECT_EQ(addr4, another_sockaddr.host());
  EXPECT_EQ(kPort, another_sockaddr.port());
}
#endif

#if 0
// Some extra reinterpret_cast restrictions in depot3
TEST(SocketAddressTest, BasicTest6) {
  const uint16 kPort = 65320;
  const uint16 kNetworkByteOrderPort = htons(kPort);

  IPAddress addr6;
  ASSERT_TRUE(StringToIPAddress("2001:700:300:1800::f", &addr6));
  SocketAddress sockaddr(addr6, kPort);

  EXPECT_EQ(addr6, sockaddr.host());
  EXPECT_EQ(kPort, sockaddr.port());

  sockaddr_in6 returned_addr6 = sockaddr.ipv6_address();
  EXPECT_EQ(AF_INET6, returned_addr6.sin6_family);
  EXPECT_EQ(addr6, IPAddress(returned_addr6.sin6_addr));
  EXPECT_EQ(kNetworkByteOrderPort, returned_addr6.sin6_port);

  std::string packed = sockaddr.ToPackedString();
  EXPECT_EQ(sizeof(returned_addr6.sin6_addr) + 2, packed.length());
  EXPECT_EQ(0, memcmp(packed.data(), &returned_addr6.sin6_addr,
                      sizeof(returned_addr6.sin6_addr)));
  EXPECT_EQ(0, memcmp(packed.data() + sizeof(returned_addr6.sin6_addr),
                      &kNetworkByteOrderPort, sizeof(kNetworkByteOrderPort)));

  EXPECT_TRUE(PackedStringToSocketAddress(packed, nullptr));
  SocketAddress unpacked;
  EXPECT_TRUE(PackedStringToSocketAddress(packed, &unpacked));
  EXPECT_EQ(sockaddr, unpacked);

  sockaddr_storage returned_addr_generic = sockaddr.generic_address();
  EXPECT_EQ(AF_INET6, returned_addr_generic.ss_family);
  sockaddr_in6* cast_addr6
      = reinterpret_cast<sockaddr_in6*>(&returned_addr_generic);

  EXPECT_EQ(addr6, IPAddress(cast_addr6->sin6_addr));
  EXPECT_EQ(kNetworkByteOrderPort, cast_addr6->sin6_port);

  // Test assignment.
  SocketAddress another_sockaddr;
  another_sockaddr = sockaddr;
  EXPECT_EQ(addr6, another_sockaddr.host());
  EXPECT_EQ(kPort, another_sockaddr.port());
}
#endif

TEST(SocketAddressTest, GenericInput4) {
  const uint16 kPort = 6502;
  const char kIPAddress[] = "1.2.3.4";

  sockaddr_in addr4;
  addr4.sin_family = AF_INET;
  inet_pton(AF_INET, kIPAddress, &addr4.sin_addr);
  addr4.sin_port = htons(kPort);

  // NOTE(sesse): Technically the cast from sockaddr_in* to sockaddr_storage*
  //              is not allowed, but the class is not looking at the bits
  //              beyond the sockaddr_in* part anyway.
  SocketAddress sockaddr1(*reinterpret_cast<sockaddr*>(&addr4));
  SocketAddress sockaddr2(*reinterpret_cast<sockaddr_storage*>(&addr4));

  ASSERT_EQ(AF_INET, sockaddr1.host().address_family());
  ASSERT_EQ(AF_INET, sockaddr2.host().address_family());
  EXPECT_EQ(kIPAddress, sockaddr1.host().ToString());
  EXPECT_EQ(kIPAddress, sockaddr2.host().ToString());
  EXPECT_EQ(kPort, sockaddr1.port());
  EXPECT_EQ(kPort, sockaddr2.port());
}

TEST(SocketAddressTest, GenericInput6) {
  const uint16 kPort = 1542;
  const char kIPAddress[] = "2001:700:300:1800::f";

  sockaddr_in6 addr6;
  addr6.sin6_family = AF_INET6;
  inet_pton(AF_INET6, kIPAddress, &addr6.sin6_addr);
  addr6.sin6_port = htons(kPort);

  // NOTE(sesse): Technically the cast from sockaddr_in6* to sockaddr_storage*
  //              is not allowed, but the class is not looking at the bits
  //              beyond the sockaddr_in6* part anyway.
  SocketAddress sockaddr1(*reinterpret_cast<sockaddr*>(&addr6));
  SocketAddress sockaddr2(*reinterpret_cast<sockaddr_storage*>(&addr6));

  ASSERT_EQ(AF_INET6, sockaddr1.host().address_family());
  ASSERT_EQ(AF_INET6, sockaddr2.host().address_family());
  EXPECT_EQ(kIPAddress, sockaddr1.host().ToString());
  EXPECT_EQ(kIPAddress, sockaddr2.host().ToString());
  EXPECT_EQ(kPort, sockaddr1.port());
  EXPECT_EQ(kPort, sockaddr2.port());
}

TEST(SocketAddressTest, EmptySockaddr) {
  sockaddr empty;
  sockaddr_storage empty_generic;

  empty.sa_family = AF_UNSPEC;
  empty_generic.ss_family = AF_UNSPEC;

  SocketAddress empty1(empty);
  SocketAddress empty2(empty_generic);

  EXPECT_EQ(AF_UNSPEC, empty1.host().address_family());
  EXPECT_EQ(AF_UNSPEC, empty2.host().address_family());
  EXPECT_EQ(empty1, empty2);
}

#if 0
// StringToSocketAddress not implemented
TEST(SocketAddressTest, ToAndFromString4) {
  const std::string kIPString = "1.2.3.4";
  const int kPort = 1234;
  const std::string kSockaddrString = absl::StrCat(kIpString, ":", kPort);
  const std::string kBogusSockaddrString1 = "1.2.3.256:1234";
  const std::string kBogusSockaddrString2 = "1.2.3.4:123456";
  const std::string kBogusSockaddrString3 = "1.2.3.4:-1";
  const std::string kBogusSockaddrString4 = "1.2.3.4:+1";
  const std::string kBogusSockaddrString5 = "1.2.3.4:";
  const std::string kBogusSockaddrString6 = "1.2.3.4:1:2";
  const std::string kBogusSockaddrString7 = "1.2.3.4:1234 ";
  const std::string kBogusSockaddrString8 = " 1.2.3.4:1234";
  const std::string kBogusSockaddrString9 = "1.2.3.4 :1234";
  const std::string kBogusSockaddrString10 = "1.2.3.4";
  const std::string kBogusSockaddrString11 = "[1.2.3.4]:5";
  const std::string kEdgeSockaddrString1 = "1.2.3.4:0";
  const std::string kEdgeSockaddrString2 = "1.2.3.4:65535";

  sockaddr_in addr4;
  addr4.sin_family = AF_INET;
  CHECK_GT(inet_pton(AF_INET, kIPString.c_str(), &addr4.sin_addr), 0);
  addr4.sin_port = htons(kPort);

  SocketAddress addr;
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString1, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString2, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString3, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString4, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString5, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString6, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString7, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString8, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString9, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString10, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString11, &addr));
  EXPECT_TRUE(StringToSocketAddress(kEdgeSockaddrString1, &addr));
  EXPECT_TRUE(StringToSocketAddress(kEdgeSockaddrString2, &addr));
  ASSERT_TRUE(StringToSocketAddress(kSockaddrString, NULL));
  ASSERT_TRUE(StringToSocketAddress(kSockaddrString, &addr));

  sockaddr_in returned_addr4 = addr.ipv4_address();
  EXPECT_EQ(addr4.sin_family, returned_addr4.sin_family);
  EXPECT_EQ(memcmp(&addr4.sin_addr,
                   &returned_addr4.sin_addr,
                   sizeof(addr4.sin_addr)),
            0);
  EXPECT_EQ(addr4.sin_port, returned_addr4.sin_port);

  EXPECT_EQ(kSockaddrString, addr.ToString());
}

TEST(SocketAddressTest, ToAndFromString6) {
  const std::string kIPString = "2001:700:300:1800::f";
  const int kPort = 50000;
  const std::string kSockaddrString = absl::StrCat("[", kIPString, "]:", kPort);
  const std::string kBogusSockaddrString1 = "[2001:700:300:180g::f]:1234";
  const std::string kBogusSockaddrString2 = "[2001:700:300:1800::f]:123456";
  const std::string kBogusSockaddrString3 = "[2001:700:300:1800::f]:-1";
  const std::string kBogusSockaddrString4 = "[2001:700:300:1800::f]:+1";
  const std::string kBogusSockaddrString5 = "[2001:700:300:1800::f]:";
  const std::string kBogusSockaddrString6 = "[2001:700:300:1800::f]:1:2";
  const std::string kBogusSockaddrString7 = "[2001:700:300:1800::f]:1234 ";
  const std::string kBogusSockaddrString8 = "[ 2001:700:300:1800::f]:1234";
  const std::string kBogusSockaddrString9 = "[2001:700:300:1800::f ]:1234";
  const std::string kBogusSockaddrString10 = "[2001:700:300:1800::f]";
  const std::string kBogusSockaddrString11 = "[2001:700:300:1800::f]]:1234";
  const std::string kBogusSockaddrString12 = "[2001:700:300:1800::f:1234";
  const std::string kEdgeSockaddrString1 = "[2001:700:300:1800::f]:0";
  const std::string kEdgeSockaddrString2 = "[2001:700:300:1800::f]:65535";

  sockaddr_in6 addr6;
  addr6.sin6_family = AF_INET6;
  CHECK_GT(inet_pton(AF_INET6, kIPString.c_str(), &addr6.sin6_addr), 0);
  addr6.sin6_port = htons(kPort);

  SocketAddress addr;
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString1, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString2, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString3, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString4, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString5, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString6, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString7, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString8, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString9, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString10, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString11, &addr));
  EXPECT_FALSE(StringToSocketAddress(kBogusSockaddrString12, &addr));
  EXPECT_TRUE(StringToSocketAddress(kEdgeSockaddrString1, &addr));
  EXPECT_TRUE(StringToSocketAddress(kEdgeSockaddrString2, &addr));
  ASSERT_TRUE(StringToSocketAddress(kSockaddrString, NULL));
  ASSERT_TRUE(StringToSocketAddress(kSockaddrString, &addr));

  sockaddr_in6 returned_addr6 = addr.ipv6_address();
  EXPECT_EQ(addr6.sin6_family, returned_addr6.sin6_family);
  EXPECT_EQ(memcmp(&addr6.sin6_addr,
                   &returned_addr6.sin6_addr,
                   sizeof(addr6.sin6_addr)),
            0);
  EXPECT_EQ(addr6.sin6_port, returned_addr6.sin6_port);

  EXPECT_EQ(kSockaddrString, addr.ToString());
}

TEST(SocketAddressTest, Equality) {
  const std::string kIPv4String1 = "1.2.3.4:1234";
  const std::string kIPv4String2 = "1.2.3.4:53764";
  const std::string kIPv6String1 = "[2001:700:300:1800::f]:1234";
  const std::string kIPv6String2 = "[2001:700:300:1800:0:0:0:f]:1234";
  const std::string kIPv6String3 = "[2001:700:300:1800::f]:53764";

  SocketAddress empty;
  SocketAddress addr4_1, addr4_2;
  SocketAddress addr6_1, addr6_2, addr6_3;

  ASSERT_TRUE(StringToSocketAddress(kIPv4String1, &addr4_1));
  ASSERT_TRUE(StringToSocketAddress(kIPv4String2, &addr4_2));
  ASSERT_TRUE(StringToSocketAddress(kIPv6String1, &addr6_1));
  ASSERT_TRUE(StringToSocketAddress(kIPv6String2, &addr6_2));
  ASSERT_TRUE(StringToSocketAddress(kIPv6String3, &addr6_3));

  // operator==
  EXPECT_TRUE(empty == empty);
  EXPECT_FALSE(empty == addr4_1);
  EXPECT_FALSE(empty == addr4_2);
  EXPECT_FALSE(empty == addr6_1);
  EXPECT_FALSE(empty == addr6_2);
  EXPECT_FALSE(empty == addr6_3);

  EXPECT_FALSE(addr4_1 == empty);
  EXPECT_TRUE(addr4_1 == addr4_1);
  EXPECT_FALSE(addr4_1 == addr4_2);
  EXPECT_FALSE(addr4_1 == addr6_1);
  EXPECT_FALSE(addr4_1 == addr6_2);
  EXPECT_FALSE(addr4_1 == addr6_3);

  EXPECT_FALSE(addr4_2 == empty);
  EXPECT_FALSE(addr4_2 == addr4_1);
  EXPECT_TRUE(addr4_2 == addr4_2);
  EXPECT_FALSE(addr4_2 == addr6_1);
  EXPECT_FALSE(addr4_2 == addr6_2);
  EXPECT_FALSE(addr4_2 == addr6_3);

  EXPECT_FALSE(addr6_1 == empty);
  EXPECT_FALSE(addr6_1 == addr4_1);
  EXPECT_FALSE(addr6_1 == addr4_2);
  EXPECT_TRUE(addr6_1 == addr6_1);
  EXPECT_TRUE(addr6_1 == addr6_2);
  EXPECT_FALSE(addr6_1 == addr6_3);

  EXPECT_FALSE(addr6_2 == empty);
  EXPECT_FALSE(addr6_2 == addr4_1);
  EXPECT_FALSE(addr6_2 == addr4_2);
  EXPECT_TRUE(addr6_2 == addr6_1);
  EXPECT_TRUE(addr6_2 == addr6_2);
  EXPECT_FALSE(addr6_2 == addr6_3);

  EXPECT_FALSE(addr6_3 == empty);
  EXPECT_FALSE(addr6_3 == addr4_1);
  EXPECT_FALSE(addr6_3 == addr4_2);
  EXPECT_FALSE(addr6_3 == addr6_1);
  EXPECT_FALSE(addr6_3 == addr6_2);
  EXPECT_TRUE(addr6_3 == addr6_3);

  // operator!= (same tests, just inverted)
  EXPECT_FALSE(empty != empty);
  EXPECT_TRUE(empty != addr4_1);
  EXPECT_TRUE(empty != addr4_2);
  EXPECT_TRUE(empty != addr6_1);
  EXPECT_TRUE(empty != addr6_2);
  EXPECT_TRUE(empty != addr6_3);

  EXPECT_TRUE(addr4_1 != empty);
  EXPECT_FALSE(addr4_1 != addr4_1);
  EXPECT_TRUE(addr4_1 != addr4_2);
  EXPECT_TRUE(addr4_1 != addr6_1);
  EXPECT_TRUE(addr4_1 != addr6_2);
  EXPECT_TRUE(addr4_1 != addr6_3);

  EXPECT_TRUE(addr4_2 != empty);
  EXPECT_TRUE(addr4_2 != addr4_1);
  EXPECT_FALSE(addr4_2 != addr4_2);
  EXPECT_TRUE(addr4_2 != addr6_1);
  EXPECT_TRUE(addr4_2 != addr6_2);
  EXPECT_TRUE(addr4_2 != addr6_3);

  EXPECT_TRUE(addr6_1 != empty);
  EXPECT_TRUE(addr6_1 != addr4_1);
  EXPECT_TRUE(addr6_1 != addr4_2);
  EXPECT_FALSE(addr6_1 != addr6_1);
  EXPECT_FALSE(addr6_1 != addr6_2);
  EXPECT_TRUE(addr6_1 != addr6_3);

  EXPECT_TRUE(addr6_2 != empty);
  EXPECT_TRUE(addr6_2 != addr4_1);
  EXPECT_TRUE(addr6_2 != addr4_2);
  EXPECT_FALSE(addr6_2 != addr6_1);
  EXPECT_FALSE(addr6_2 != addr6_2);
  EXPECT_TRUE(addr6_2 != addr6_3);

  EXPECT_TRUE(addr6_3 != empty);
  EXPECT_TRUE(addr6_3 != addr4_1);
  EXPECT_TRUE(addr6_3 != addr4_2);
  EXPECT_TRUE(addr6_3 != addr6_1);
  EXPECT_TRUE(addr6_3 != addr6_2);
  EXPECT_FALSE(addr6_3 != addr6_3);
}

// Invalid SocketAddress conversion in *OrDie functions.
TEST(SocketAddressDeathTest, InvalidSocketAddressString) {
  EXPECT_DEATH(StringToSocketAddressOrDie("foo"), "Invalid SocketAddress foo");
  EXPECT_DEATH(StringToSocketAddressOrDie(std::string("172.1.1.100")),
               "Invalid SocketAddress");
  EXPECT_DEATH(StringToSocketAddressOrDie("172.1.1.100:-1"),
               "Invalid SocketAddress");
  EXPECT_DEATH(StringToSocketAddressOrDie("172.1.1.100:test"),
               "Invalid SocketAddress");
  EXPECT_DEATH(StringToSocketAddressOrDie("172.1.1.100:65536"),
               "Invalid SocketAddress");
  EXPECT_DEATH(StringToSocketAddressOrDie("2001:700:300:183::1:1"),
               "Invalid SocketAddress");
  EXPECT_DEATH(StringToSocketAddressOrDie("[2001:700:300:183::g]"),
               "Invalid SocketAddress");
  EXPECT_DEATH(StringToSocketAddressOrDie("[2001:700:300:183::1]:"),
               "Invalid SocketAddress");
  EXPECT_DEATH(StringToSocketAddressOrDie("[2001:700:300:183::1]:foo"),
               "Invalid SocketAddress");
  EXPECT_DEATH(StringToSocketAddressOrDie("[2001:700:300:183::1]:-1"),
               "Invalid SocketAddress");
  EXPECT_DEATH(StringToSocketAddressOrDie("[2001:700:300:183::1]:65536"),
               "Invalid SocketAddress");

  EXPECT_EQ(StringToSocketAddressOrDie("1.2.3.4:5").ToString(), "1.2.3.4:5");
  EXPECT_EQ(StringToSocketAddressOrDie("[::1]:6").ToString(), "[::1]:6");
}

TEST(SocketAddressTest, FromStringWithDefaultPort4) {
  const int kDefaultPort = 50000;
  const std::string kBogusSockaddrString1 = "1.2.3.256:1234";
  const std::string kBogusSockaddrString2 = "1.2.3.4:123456";
  const std::string kBogusSockaddrString3 = "1.2.3.4:-1";
  const std::string kBogusSockaddrString4 = "1.2.3.4:+1";
  const std::string kBogusSockaddrString5 = "1.2.3.4:";
  const std::string kBogusSockaddrString6 = "1.2.3.4:1:2";
  const std::string kBogusSockaddrString7 = "1.2.3.4:1234 ";
  const std::string kBogusSockaddrString8 = " 1.2.3.4:1234";
  const std::string kBogusSockaddrString9 = "1.2.3.4 :1234";
  const std::string kSockaddrStringWithoutPort = "1.2.3.4";
  const std::string kEdgeSockaddrString1 = "1.2.3.4:0";
  const std::string kEdgeSockaddrString2 = "1.2.3.4:65535";

  SocketAddress addr;
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString1, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString2, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString3, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString4, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString5, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString6, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString7, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString8, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString9, kDefaultPort, &addr));

  EXPECT_TRUE(StringToSocketAddressWithDefaultPort(
      kSockaddrStringWithoutPort, kDefaultPort, &addr));
  EXPECT_EQ(kDefaultPort, addr.port());
  EXPECT_TRUE(StringToSocketAddressWithDefaultPort(
      kEdgeSockaddrString1, kDefaultPort, &addr));
  EXPECT_EQ(0, addr.port());
  EXPECT_TRUE(StringToSocketAddressWithDefaultPort(
      kEdgeSockaddrString2, kDefaultPort, &addr));
  EXPECT_EQ(65535, addr.port());
}

TEST(SocketAddressTest, FromStringWithDefaultPort6) {
  const int kDefaultPort = 50000;
  const std::string kBogusSockaddrString1 = "[2001:700:300:180g::f]:1234";
  const std::string kBogusSockaddrString2 = "[2001:700:300:1800::f]:123456";
  const std::string kBogusSockaddrString3 = "[2001:700:300:1800::f]:-1";
  const std::string kBogusSockaddrString4 = "[2001:700:300:1800::f]:+1";
  const std::string kBogusSockaddrString5 = "[2001:700:300:1800::f]:";
  const std::string kBogusSockaddrString6 = "[2001:700:300:1800::f]:1:2";
  const std::string kBogusSockaddrString7 = "[2001:700:300:1800::f]:1234 ";
  const std::string kBogusSockaddrString8 = "[ 2001:700:300:1800::f]:1234";
  const std::string kBogusSockaddrString9 = "[2001:700:300:1800::f ]:1234";
  const std::string kBogusSockaddrString10 = "[2001:700:300:1800::f]]:1234";
  const std::string kBogusSockaddrString11 = "[2001:700:300:1800::f:1234";
  const std::string kSockaddrStringWithoutPort = "[2001:700:300:1800::f]";
  const std::string kEdgeSockaddrString1 = "[2001:700:300:1800::f]:0";
  const std::string kEdgeSockaddrString2 = "[2001:700:300:1800::f]:65535";

  SocketAddress addr;
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString1, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString2, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString3, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString4, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString5, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString6, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString7, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString8, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString9, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString10, kDefaultPort, &addr));
  EXPECT_FALSE(StringToSocketAddressWithDefaultPort(
      kBogusSockaddrString11, kDefaultPort, &addr));

  EXPECT_TRUE(StringToSocketAddressWithDefaultPort(
      kSockaddrStringWithoutPort, kDefaultPort, &addr));
  EXPECT_EQ(kDefaultPort, addr.port());
  EXPECT_TRUE(StringToSocketAddressWithDefaultPort(
      kEdgeSockaddrString1, kDefaultPort, &addr));
  EXPECT_EQ(0, addr.port());
  EXPECT_TRUE(StringToSocketAddressWithDefaultPort(
      kEdgeSockaddrString2, kDefaultPort, &addr));
  EXPECT_EQ(65535, addr.port());
}

TEST(SocketAddressTest, Logging) {
  const std::string kIPv4String = "1.2.3.4:1337";
  const std::string kIPv6String = "[2001:700:300:1800::f]:1338";
  SocketAddress sockaddr4, sockaddr6;

  ASSERT_TRUE(StringToSocketAddress(kIPv4String, &sockaddr4));
  ASSERT_TRUE(StringToSocketAddress(kIPv6String, &sockaddr6));

  std::ostringstream out;
  out << sockaddr4 << " " << sockaddr6;
  EXPECT_EQ("1.2.3.4:1337 [2001:700:300:1800::f]:1338", out.str());
}

TEST(SocketAddressTest, LoggingUninitialized) {
  std::ostringstream out;
  out << SocketAddress();
  EXPECT_EQ("<uninitialized SocketAddress>", out.str());
}

TEST(SocketAddressTest, Joining) {
  std::vector<SocketAddress> v = {
      StringToSocketAddressOrDie("192.0.2.0:80"),
      StringToSocketAddressOrDie("[2001:db8::]:443"),
      StringToSocketAddressOrDie("0.0.0.0:80"),
      StringToSocketAddressOrDie("[::]:443")
  };
  EXPECT_EQ("192.0.2.0:80***[2001:db8::]:443***0.0.0.0:80***[::]:443",
            std::strings::Join(v, "***", SocketAddressJoinFormatter()));
}
#endif

TEST(SocketAddressTest, SocketAddressOrdering) {
  const std::string kIPString1 = "1.2.3.4";
  const std::string kIPString2 = "4.3.2.1";

  IPAddress addr1, addr2;

  ASSERT_TRUE(StringToIPAddress(kIPString1, &addr1));
  ASSERT_TRUE(StringToIPAddress(kIPString2, &addr2));

  SocketAddress sock_addr0;
  SocketAddress sock_addr1(addr1, 5);
  SocketAddress sock_addr2(addr2, 3);
  SocketAddress sock_addr3(addr1, 4);
  SocketAddress sock_addr4(addr2, 8);
  SocketAddress sock_addr5(addr1, 40000);  // port >= 2^15 to check signness.

  std::set<SocketAddress, SocketAddressOrdering> sock_addrs;
  sock_addrs.insert(sock_addr1);
  sock_addrs.insert(sock_addr2);
  sock_addrs.insert(sock_addr3);
  sock_addrs.insert(sock_addr4);
  sock_addrs.insert(sock_addr5);
  sock_addrs.insert(sock_addr0);

  EXPECT_EQ(6u, sock_addrs.size());

  std::vector<SocketAddress> sorted_sock_addrs(sock_addrs.begin(), sock_addrs.end());
  ASSERT_EQ(6u, sorted_sock_addrs.size());
  EXPECT_EQ(sock_addr0, sorted_sock_addrs[0]);
  EXPECT_EQ(sock_addr3, sorted_sock_addrs[1]);
  EXPECT_EQ(sock_addr1, sorted_sock_addrs[2]);
  EXPECT_EQ(sock_addr5, sorted_sock_addrs[3]);
  EXPECT_EQ(sock_addr2, sorted_sock_addrs[4]);
  EXPECT_EQ(sock_addr4, sorted_sock_addrs[5]);
}

TEST(SocketAddressTest, Hash) {
  const std::string kIPString1 = "1.2.3.4";
  const std::string kIPString2 = "4.3.2.1";

  IPAddress addr1, addr2;

  ASSERT_TRUE(StringToIPAddress(kIPString1, &addr1));
  ASSERT_TRUE(StringToIPAddress(kIPString2, &addr2));

  SocketAddress sock_addr0;
  SocketAddress sock_addr1(addr1, 5);
  SocketAddress sock_addr2(addr2, 3);
  SocketAddress sock_addr3(addr1, 4);
  SocketAddress sock_addr4(addr2, 8);
  SocketAddress sock_addr5(addr1, 40000);  // port >= 2^15 to check signness.

  hash_set<SocketAddress> sock_addrs;
  sock_addrs.insert(sock_addr0);
  sock_addrs.insert(SocketAddress());
  sock_addrs.insert(sock_addr1);
  sock_addrs.insert(sock_addr2);
  sock_addrs.insert(sock_addr3);
  sock_addrs.insert(sock_addr4);
  sock_addrs.insert(sock_addr5);

  EXPECT_EQ(6u, sock_addrs.size());

  EXPECT_EQ(1u, sock_addrs.count(sock_addr0));
  EXPECT_EQ(1u, sock_addrs.count(sock_addr1));
  EXPECT_EQ(1u, sock_addrs.count(sock_addr2));
  EXPECT_EQ(1u, sock_addrs.count(sock_addr3));
  EXPECT_EQ(1u, sock_addrs.count(sock_addr4));
  EXPECT_EQ(1u, sock_addrs.count(sock_addr5));
}

#if 0
// StringToSocketAddress not supported
TEST(SocketAddressTest, NormalizeSocketAddress) {
  SocketAddress addr4, mapped_addr, compat_addr;

  ASSERT_TRUE(StringToSocketAddress("129.241.93.35:21", &addr4));
  ASSERT_TRUE(StringToSocketAddress("[::ffff:129.241.93.35]:21", &mapped_addr));
  ASSERT_TRUE(StringToSocketAddress("[::129.241.93.35]:21", &compat_addr));

  EXPECT_EQ(addr4, NormalizeSocketAddress(addr4));
  EXPECT_EQ(addr4, NormalizeSocketAddress(mapped_addr));
  EXPECT_EQ(compat_addr, NormalizeSocketAddress(compat_addr));

  SocketAddress addr6, loopback_addr6;
  ASSERT_TRUE(StringToSocketAddress("[2001:700:300:1803::1]:21", &addr6));
  ASSERT_TRUE(StringToSocketAddress("[::1]:21", &loopback_addr6));
  EXPECT_EQ(addr6, NormalizeSocketAddress(addr6));
  EXPECT_EQ(loopback_addr6, NormalizeSocketAddress(loopback_addr6));
}

TEST(SocketAddressTest, DualstackSocketAddress) {
  SocketAddress addr4 = StringToSocketAddressOrDie("192.0.2.1:21");
  SocketAddress mapped_addr =
      StringToSocketAddressOrDie("[::ffff:192.0.2.1]:21");
  SocketAddress compat_addr = StringToSocketAddressOrDie("[::192.0.2.1]:21");

  EXPECT_EQ(mapped_addr, DualstackSocketAddress(addr4));
  EXPECT_EQ(mapped_addr, DualstackSocketAddress(mapped_addr));
  EXPECT_EQ(compat_addr, DualstackSocketAddress(compat_addr));

  EXPECT_EQ(StringToSocketAddressOrDie("[::ffff:127.0.0.1]:123"),
            DualstackSocketAddress(SocketAddress(IPAddress::Loopback4(), 123)));
  EXPECT_EQ(StringToSocketAddressOrDie("[::ffff:0.0.0.0]:123"),
            DualstackSocketAddress(SocketAddress(IPAddress::Any4(), 123)));

  SocketAddress addr6 = StringToSocketAddressOrDie("[2001:db8::1]:21");
  EXPECT_EQ(addr6, DualstackSocketAddress(addr6));
  SocketAddress loopback6 = StringToSocketAddressOrDie("[::1]:21");
  EXPECT_EQ(loopback6, DualstackSocketAddress(loopback6));
  SocketAddress any6 = StringToSocketAddressOrDie("[::]:21");
  EXPECT_EQ(any6, DualstackSocketAddress(any6));
}

TEST(SocketAddressTest, IsInitializedSocketAddress) {
  SocketAddress uninit_addr, addr4, addr6;

  EXPECT_FALSE(IsInitializedSocketAddress(uninit_addr));
  EXPECT_FALSE(IsInitializedSocketAddress(addr4));
  EXPECT_FALSE(IsInitializedSocketAddress(addr6));

  ASSERT_TRUE(StringToSocketAddress("129.241.93.35:4919", &addr4));
  ASSERT_TRUE(StringToSocketAddress("[2001:67c:a4::1]:4919", &addr6));

  EXPECT_FALSE(IsInitializedSocketAddress(uninit_addr));
  EXPECT_TRUE(IsInitializedSocketAddress(addr4));
  EXPECT_TRUE(IsInitializedSocketAddress(addr6));
}

#endif

TEST(SocketAddressDeathTest, UninitializedGenericAddress) {
  SocketAddress empty;
  EXPECT_DEATH(empty.generic_address(),
               "uninitialized SocketAddress");
}

TEST(SocketAddressDeathTest, EmergencyZeroPort) {
  SocketAddress empty;

  EXPECT_DEATH(empty.port(), "empty SocketAddress");
}

TEST(SocketAddressDeathTest, EmergencyEmptyString) {
  SocketAddress empty;

  EXPECT_DEATH(empty.ToString(), "empty SocketAddress");
}

// Tests for IPRange.
TEST(IPRangeTest, BasicTest4) {
  IPAddress addr;
  const uint16 kPrefixLength = 16;
  ASSERT_TRUE(StringToIPAddress("192.168.0.0", &addr));
  IPRange subnet(addr, kPrefixLength);
  EXPECT_EQ(addr, subnet.host());
  EXPECT_EQ(kPrefixLength, subnet.length());

  // Test copy construction.
  IPRange another_subnet = subnet;
  EXPECT_EQ(addr, another_subnet.host());
  EXPECT_EQ(kPrefixLength, another_subnet.length());

  // Test IPAddress constructor.
  EXPECT_EQ(addr, IPRange(addr).host());
  EXPECT_EQ(32, IPRange(addr).length());
}

TEST(IPRangeTest, BasicTest6) {
  IPAddress addr;
  const uint16 kPrefixLength = 64;
  ASSERT_TRUE(StringToIPAddress("2001:700:300:1800::", &addr));
  IPRange subnet(addr, kPrefixLength);
  EXPECT_EQ(addr, subnet.host());
  EXPECT_EQ(kPrefixLength, subnet.length());

  // Test copy construction.
  IPRange another_subnet = subnet;
  EXPECT_EQ(addr, another_subnet.host());
  EXPECT_EQ(kPrefixLength, another_subnet.length());

  // Test IPAddress constructor.
  EXPECT_EQ(addr, IPRange(addr).host());
  EXPECT_EQ(128, IPRange(addr).length());
}

TEST(IPRangeTest, AnyRanges) {
  EXPECT_EQ("0.0.0.0/0", IPRange::Any4().ToString());
  EXPECT_EQ("::/0", IPRange::Any6().ToString());
}

TEST(IPRangeTest, ToAndFromString4) {
  const std::string kIPString = "192.168.0.0";
  const int kLength = 16;
  const std::string kSubnetString = absl::StrCat(kIPString, "/", kLength);
  const std::string kBogusSubnetString1 = "192.168.0.0/8";
  const std::string kBogusSubnetString2 = "192.256.0.0/16";
  const std::string kBogusSubnetString3 = "192.168.0.0/34";
  const std::string kBogusSubnetString4 = "0.0.0.0/-1";
  const std::string kBogusSubnetString5 = "0.0.0.0/+1";
  const std::string kBogusSubnetString6 = "0.0.0.0/";
  const std::string kBogusSubnetString7 = "192.168.0.0/16/16";
  const std::string kBogusSubnetString8 = "192.168.0.0/16 ";
  const std::string kBogusSubnetString9 = " 192.168.0.0/16";
  const std::string kBogusSubnetString10 = "192.168.0.0 /16";

  IPRange subnet;
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString1, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString2, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString3, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString4, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString5, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString6, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString7, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString8, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString9, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString10, &subnet));
  ASSERT_TRUE(StringToIPRange(kSubnetString, NULL));
  ASSERT_TRUE(StringToIPRange(kSubnetString, &subnet));

  IPAddress addr4;
  ASSERT_TRUE(StringToIPAddress(kIPString, &addr4));
  EXPECT_EQ(addr4, subnet.host());
  EXPECT_EQ(kLength, subnet.length());

  EXPECT_EQ(kSubnetString, subnet.ToString());

  EXPECT_TRUE(StringToIPRangeAndTruncate(kBogusSubnetString1, &subnet));
  EXPECT_EQ("192.0.0.0/8", subnet.ToString());
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString2, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString3, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString4, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString5, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString6, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString7, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString8, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString9, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString10, &subnet));
}

TEST(IPRangeTest, DottedQuadNetmasks) {
  const std::string kIPString = "192.168.0.0";
  const std::string kDottedQuadNetmaskString = "255.255.0.0";
  const int kLength = 16;
  const std::string kSubnetString = absl::StrCat(kIPString, "/", kLength);
  const std::string kDottedQuadSubnetString =
      absl::StrCat(kIPString, "/", kDottedQuadNetmaskString);

  const std::string kBogusDottedQuadStrings[] = {
      "192.168.0.0/128.255.0.0",
      "3ffe::1/255.255.0.0",
      "1.2.3.4/255",
      "1.2.3.4/255.",
      "1.2.3.4/255.255",
      "1.2.3.4/255.255.",
      "1.2.3.4/255.255.255",
      "1.2.3.4/255.255.255.",
      "1.2.3.4/255.255.255.256",
      "1.2.3.4/255.255.255.-255",
      "1.2.3.4/255.255.255.+255",
      "1.2.3.4/255.255.255.garbage",
      "1.2.3.4/0255.255.255.255",
      "1.2.3.4/255.255.255.000255",
  };

  // Check bogus std::strings.
  for (size_t i = 0; i < ARRAYSIZE(kBogusDottedQuadStrings); ++i) {
    const std::string& bogus = kBogusDottedQuadStrings[i];
    EXPECT_FALSE(StringToIPRangeAndTruncate(bogus, NULL))
        << "Apparently '" << bogus << "' is actually valid?";
  }

  // Check valid std::strings.
  IPRange cidr;
  IPRange dotted_quad;
  ASSERT_TRUE(StringToIPRangeAndTruncate(kSubnetString, &cidr));
  ASSERT_TRUE(StringToIPRangeAndTruncate(kDottedQuadSubnetString,
                                         &dotted_quad));
  ASSERT_TRUE(cidr == dotted_quad);

  // Check some corner cases.
  EXPECT_TRUE(StringToIPRange("0.0.0.0/0.0.0.0", &cidr));
  EXPECT_EQ(0, cidr.length());
  EXPECT_EQ(IPAddress::Any4(), cidr.host());

  ASSERT_TRUE(StringToIPRange("127.0.0.1/255.255.255.255", &cidr));
  EXPECT_EQ(32, cidr.length());
  EXPECT_EQ(IPAddress::Loopback4(), cidr.host());

  // If .expected_host_string is empty then .dotted_quad_string is
  // expected to FAIL StringToIPRangeAndTruncate().
  const struct DottedQuadExpecations {
    std::string dotted_quad_string;
    std::string expected_host_string;
    int expected_length;
  } dotted_quad_tests[] = {
    { "1.2.3.4/0.0.0.1", "", -1 },
    { "1.2.3.4/1.0.0.0", "", -1 },
    { "1.2.3.4/127.255.255.255", "", -1 },
    { "1.2.3.4/254.255.255.255", "", -1 },
    { "1.2.3.4/255.255.255.254", "1.2.3.4", 31 },
    { "1.2.3.4/0.0.0.0", "0.0.0.0", 0 },
  };

  for (size_t i = 0; i < ARRAYSIZE(dotted_quad_tests); ++i) {
    IPRange range;
    IPAddress host;

    if (dotted_quad_tests[i].expected_host_string.empty()) {
      // The dotted quad std::string should be rejected as invalid.
      ASSERT_FALSE(StringToIPRangeAndTruncate(
          dotted_quad_tests[i].dotted_quad_string, &range));
      continue;
    }
    ASSERT_TRUE(StringToIPRangeAndTruncate(
        dotted_quad_tests[i].dotted_quad_string, &range));
    ASSERT_TRUE(StringToIPAddress(dotted_quad_tests[i].expected_host_string,
                                  &host));
    EXPECT_EQ(host, range.host())
        << dotted_quad_tests[i].dotted_quad_string
        << " host equality expectation failed";
    EXPECT_EQ(dotted_quad_tests[i].expected_length, range.length())
        << dotted_quad_tests[i].dotted_quad_string
        << " length equality expectation failed";
  }
}

TEST(IPRangeTest, FromAddressString4) {
  const std::string kIPString = "192.168.0.0";
  IPAddress addr4;
  ASSERT_TRUE(StringToIPAddress(kIPString, &addr4));

  IPRange subnet;
  EXPECT_TRUE(StringToIPRange(kIPString, &subnet));
  EXPECT_EQ(addr4, subnet.host());
  EXPECT_EQ(32, subnet.length());

  EXPECT_TRUE(StringToIPRangeAndTruncate(kIPString, &subnet));
  EXPECT_EQ(addr4, subnet.host());
  EXPECT_EQ(32, subnet.length());
}

TEST(IPRangeTest, ToAndFromString6) {
  const std::string kIPString = "2001:700:300:1800::";
  const int kLength = 64;
  const std::string kSubnetString = absl::StrCat(kIPString, "/", kLength);
  const std::string kBogusSubnetString1 = "2001:700:300:1800::/48";
  const std::string kBogusSubnetString2 = "2001:700:300:180g::/64";
  const std::string kBogusSubnetString3 = "2001:700:300:1800::/129";
  const std::string kBogusSubnetString4 = "::/-1";
  const std::string kBogusSubnetString5 = "::/+1";
  const std::string kBogusSubnetString6 = "::/";
  const std::string kBogusSubnetString7 = "2001:700:300:1800::/64/64";
  const std::string kBogusSubnetString8 = "2001:700:300:1800::/64 ";
  const std::string kBogusSubnetString9 = " 2001:700:300:1800::/64";
  const std::string kBogusSubnetString10 = "2001:700:300:1800:: /64";

  IPRange subnet;
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString1, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString2, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString3, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString4, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString5, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString6, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString7, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString8, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString9, &subnet));
  EXPECT_FALSE(StringToIPRange(kBogusSubnetString10, &subnet));
  ASSERT_TRUE(StringToIPRange(kSubnetString, NULL));
  ASSERT_TRUE(StringToIPRange(kSubnetString, &subnet));

  IPAddress addr6;
  ASSERT_TRUE(StringToIPAddress(kIPString, &addr6));
  EXPECT_EQ(addr6, subnet.host());
  EXPECT_EQ(kLength, subnet.length());

  EXPECT_EQ(kSubnetString, subnet.ToString());

  EXPECT_TRUE(StringToIPRangeAndTruncate(kBogusSubnetString1, &subnet));
  EXPECT_EQ("2001:700:300::/48", subnet.ToString());
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString2, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString3, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString4, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString5, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString6, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString7, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString8, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString9, &subnet));
  EXPECT_FALSE(StringToIPRangeAndTruncate(kBogusSubnetString10, &subnet));
}

TEST(IPRangeTest, FromAddressString6) {
  const std::string kIPString = "2001:700:300:1800::";
  IPAddress addr6;
  ASSERT_TRUE(StringToIPAddress(kIPString, &addr6));

  IPRange subnet;
  EXPECT_TRUE(StringToIPRange(kIPString, &subnet));
  EXPECT_EQ(addr6, subnet.host());
  EXPECT_EQ(128, subnet.length());

  EXPECT_TRUE(StringToIPRangeAndTruncate(kIPString, &subnet));
  EXPECT_EQ(addr6, subnet.host());
  EXPECT_EQ(128, subnet.length());
}

#if 0
// No std::strings::Join
TEST(IPRangeTest, Joining) {
  std::vector<IPRange> v = {
      StringToIPRangeAndTruncateOrDie("192.0.2.0/24"),
      StringToIPRangeAndTruncateOrDie("2001:db8::/32"),
      StringToIPRangeAndTruncateOrDie("0.0.0.0/0"),
      StringToIPRangeAndTruncateOrDie("::/0")
  };
  EXPECT_EQ("192.0.2.0/24 <> 2001:db8::/32 <> 0.0.0.0/0 <> ::/0",
            std::strings::Join(v, " <> ", IPRangeJoinFormatter()));
}
#endif

TEST(IPRangeTest, Equality) {
  const std::string kIPv4String1 = "192.168.0.0/16";
  const std::string kIPv4String2 = "192.168.0.0/24";
  const std::string kIPv6String1 = "2001:700:300:1800::/64";
  const std::string kIPv6String2 = "2001:700:300:1800:0:0::/64";
  const std::string kIPv6String3 = "2001:700:300:dc0f::/64";

  IPRange subnet4_1, subnet4_2;
  IPRange subnet6_1, subnet6_2, subnet6_3;

  ASSERT_TRUE(StringToIPRange(kIPv4String1, &subnet4_1));
  ASSERT_TRUE(StringToIPRange(kIPv4String2, &subnet4_2));
  ASSERT_TRUE(StringToIPRange(kIPv6String1, &subnet6_1));
  ASSERT_TRUE(StringToIPRange(kIPv6String2, &subnet6_2));
  ASSERT_TRUE(StringToIPRange(kIPv6String3, &subnet6_3));

  // operator==
  EXPECT_TRUE(subnet4_1 == subnet4_1);
  EXPECT_FALSE(subnet4_1 == subnet4_2);
  EXPECT_FALSE(subnet4_1 == subnet6_1);
  EXPECT_FALSE(subnet4_1 == subnet6_2);
  EXPECT_FALSE(subnet4_1 == subnet6_3);

  EXPECT_FALSE(subnet4_2 == subnet4_1);
  EXPECT_TRUE(subnet4_2 == subnet4_2);
  EXPECT_FALSE(subnet4_2 == subnet6_1);
  EXPECT_FALSE(subnet4_2 == subnet6_2);
  EXPECT_FALSE(subnet4_2 == subnet6_3);

  EXPECT_FALSE(subnet6_1 == subnet4_1);
  EXPECT_FALSE(subnet6_1 == subnet4_2);
  EXPECT_TRUE(subnet6_1 == subnet6_1);
  EXPECT_TRUE(subnet6_1 == subnet6_2);
  EXPECT_FALSE(subnet6_1 == subnet6_3);

  EXPECT_FALSE(subnet6_2 == subnet4_1);
  EXPECT_FALSE(subnet6_2 == subnet4_2);
  EXPECT_TRUE(subnet6_2 == subnet6_1);
  EXPECT_TRUE(subnet6_2 == subnet6_2);
  EXPECT_FALSE(subnet6_2 == subnet6_3);

  EXPECT_FALSE(subnet6_3 == subnet4_1);
  EXPECT_FALSE(subnet6_3 == subnet4_2);
  EXPECT_FALSE(subnet6_3 == subnet6_1);
  EXPECT_FALSE(subnet6_3 == subnet6_2);
  EXPECT_TRUE(subnet6_3 == subnet6_3);

  // operator!= (same tests, just inverted)
  EXPECT_FALSE(subnet4_1 != subnet4_1);
  EXPECT_TRUE(subnet4_1 != subnet4_2);
  EXPECT_TRUE(subnet4_1 != subnet6_1);
  EXPECT_TRUE(subnet4_1 != subnet6_2);
  EXPECT_TRUE(subnet4_1 != subnet6_3);

  EXPECT_TRUE(subnet4_2 != subnet4_1);
  EXPECT_FALSE(subnet4_2 != subnet4_2);
  EXPECT_TRUE(subnet4_2 != subnet6_1);
  EXPECT_TRUE(subnet4_2 != subnet6_2);
  EXPECT_TRUE(subnet4_2 != subnet6_3);

  EXPECT_TRUE(subnet6_1 != subnet4_1);
  EXPECT_TRUE(subnet6_1 != subnet4_2);
  EXPECT_FALSE(subnet6_1 != subnet6_1);
  EXPECT_FALSE(subnet6_1 != subnet6_2);
  EXPECT_TRUE(subnet6_1 != subnet6_3);

  EXPECT_TRUE(subnet6_2 != subnet4_1);
  EXPECT_TRUE(subnet6_2 != subnet4_2);
  EXPECT_FALSE(subnet6_2 != subnet6_1);
  EXPECT_FALSE(subnet6_2 != subnet6_2);
  EXPECT_TRUE(subnet6_2 != subnet6_3);

  EXPECT_TRUE(subnet6_3 != subnet4_1);
  EXPECT_TRUE(subnet6_3 != subnet4_2);
  EXPECT_TRUE(subnet6_3 != subnet6_1);
  EXPECT_TRUE(subnet6_3 != subnet6_2);
  EXPECT_FALSE(subnet6_3 != subnet6_3);
}

TEST(IPRangeTest, LowerAndUpper4) {
  IPAddress expected, ip;
  IPRange range;

  ASSERT_TRUE(StringToIPAddress("1.2.3.4", &ip));

  // 1.2.3.4/0
  range = IPRange(ip, 0);
  ASSERT_TRUE(StringToIPAddress("0.0.0.0", &expected));
  EXPECT_EQ(expected, range.host());
  EXPECT_EQ(expected, range.network_address());
  ASSERT_TRUE(StringToIPAddress("255.255.255.255", &expected));
  EXPECT_EQ(expected, range.broadcast_address());

  // 1.2.3.4/25
  range = IPRange(ip, 25);
  ASSERT_TRUE(StringToIPAddress("1.2.3.0", &expected));
  EXPECT_EQ(expected, range.host());
  EXPECT_EQ(expected, range.network_address());
  ASSERT_TRUE(StringToIPAddress("1.2.3.127", &expected));
  EXPECT_EQ(expected, range.broadcast_address());

  // 1.2.3.4/31
  range = IPRange(ip, 31);
  EXPECT_EQ(ip, range.host());
  EXPECT_EQ(ip, range.network_address());
  ASSERT_TRUE(StringToIPAddress("1.2.3.5", &expected));
  EXPECT_EQ(expected, range.broadcast_address());

  // 1.2.3.4/32
  range = IPRange(ip, 32);
  EXPECT_EQ(ip, range.host());
  EXPECT_EQ(ip, range.network_address());
  EXPECT_EQ(ip, range.broadcast_address());
}

TEST(IPRangeTest, LowerAndUpper6) {
  IPAddress expected, ip;
  IPRange range;

  ASSERT_TRUE(StringToIPAddress("1:2:3:4:5:6:7:8", &ip));

  // 1:2:3:4:5:6:7:8/0
  range = IPRange(ip, 0);
  ASSERT_TRUE(StringToIPAddress("::", &expected));
  EXPECT_EQ(expected, range.host());
  EXPECT_EQ(expected, range.network_address());
  ASSERT_TRUE(StringToIPAddress("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
                                &expected));
  EXPECT_EQ(expected, range.broadcast_address());

  // 1:2:3:4:5:6:7:8/113
  range = IPRange(ip, 113);
  ASSERT_TRUE(StringToIPAddress("1:2:3:4:5:6:7:0", &expected));
  EXPECT_EQ(expected, range.host());
  EXPECT_EQ(expected, range.network_address());
  ASSERT_TRUE(StringToIPAddress("1:2:3:4:5:6:7:7fff", &expected));
  EXPECT_EQ(expected, range.broadcast_address());

  // 1:2:3:4:5:6:7:8/127
  range = IPRange(ip, 127);
  EXPECT_EQ(ip, range.host());
  EXPECT_EQ(ip, range.network_address());
  ASSERT_TRUE(StringToIPAddress("1:2:3:4:5:6:7:9", &expected));
  EXPECT_EQ(expected, range.broadcast_address());

  // 1:2:3:4:5:6:7:8/128
  range = IPRange(ip, 128);
  EXPECT_EQ(ip, range.host());
  EXPECT_EQ(ip, range.network_address());
  EXPECT_EQ(ip, range.broadcast_address());
}

TEST(IPRangeTest, IsWithinSubnet) {
  const IPRange subnet1 = StringToIPRangeOrDie("192.168.0.0/16");
  const IPRange subnet2 = StringToIPRangeOrDie("192.168.0.0/24");
  const IPRange subnet3 = StringToIPRangeOrDie("2001:700:300:1800::/64");
  const IPRange subnet4 = StringToIPRangeOrDie("::/0");

  const IPAddress addr1 = StringToIPAddressOrDie("192.168.1.5");
  const IPAddress addr2 = StringToIPAddressOrDie("2001:700:300:1800::1");
  const IPAddress addr3 = StringToIPAddressOrDie("2001:700:300:1801::1");

  EXPECT_TRUE(IsWithinSubnet(subnet1, addr1));
  EXPECT_FALSE(IsWithinSubnet(subnet2, addr1));
  EXPECT_FALSE(IsWithinSubnet(subnet3, addr1));
  EXPECT_FALSE(IsWithinSubnet(subnet4, addr1));

  EXPECT_FALSE(IsWithinSubnet(subnet1, addr2));
  EXPECT_FALSE(IsWithinSubnet(subnet2, addr2));
  EXPECT_TRUE(IsWithinSubnet(subnet3, addr2));
  EXPECT_TRUE(IsWithinSubnet(subnet4, addr2));

  EXPECT_FALSE(IsWithinSubnet(subnet1, addr3));
  EXPECT_FALSE(IsWithinSubnet(subnet2, addr3));
  EXPECT_FALSE(IsWithinSubnet(subnet3, addr3));
  EXPECT_TRUE(IsWithinSubnet(subnet4, addr3));
}

TEST(IPRangeTest, IsProperSubRange) {
  const std::string kRangeString[] = {
      "192.168.0.0/15",  "192.169.0.0/16", "192.168.0.0/24",
      "192.168.0.80/28", "::/0",           "2001:700:300:1800::/64",
  };

  IPRange range[ABSL_ARRAYSIZE(kRangeString)];
  for (size_t i = 0; i < ABSL_ARRAYSIZE(kRangeString); ++i) {
    ASSERT_TRUE(StringToIPRange(kRangeString[i], &range[i]));
    EXPECT_FALSE(IsProperSubRange(range[i], range[i]));
  }

  EXPECT_TRUE(IsProperSubRange(range[0], range[1]));
  EXPECT_TRUE(IsProperSubRange(range[0], range[2]));
  EXPECT_TRUE(IsProperSubRange(range[0], range[3]));
  EXPECT_FALSE(IsProperSubRange(range[0], range[4]));
  EXPECT_FALSE(IsProperSubRange(range[0], range[5]));

  EXPECT_FALSE(IsProperSubRange(range[1], range[0]));
  EXPECT_FALSE(IsProperSubRange(range[1], range[2]));
  EXPECT_FALSE(IsProperSubRange(range[1], range[3]));
  EXPECT_FALSE(IsProperSubRange(range[1], range[4]));
  EXPECT_FALSE(IsProperSubRange(range[1], range[5]));

  EXPECT_FALSE(IsProperSubRange(range[2], range[0]));
  EXPECT_FALSE(IsProperSubRange(range[2], range[1]));
  EXPECT_TRUE(IsProperSubRange(range[2], range[3]));
  EXPECT_FALSE(IsProperSubRange(range[2], range[4]));
  EXPECT_FALSE(IsProperSubRange(range[2], range[5]));

  EXPECT_FALSE(IsProperSubRange(range[3], range[0]));
  EXPECT_FALSE(IsProperSubRange(range[3], range[1]));
  EXPECT_FALSE(IsProperSubRange(range[3], range[2]));
  EXPECT_FALSE(IsProperSubRange(range[3], range[4]));
  EXPECT_FALSE(IsProperSubRange(range[3], range[5]));

  EXPECT_FALSE(IsProperSubRange(range[4], range[0]));
  EXPECT_FALSE(IsProperSubRange(range[4], range[1]));
  EXPECT_FALSE(IsProperSubRange(range[4], range[2]));
  EXPECT_FALSE(IsProperSubRange(range[4], range[3]));
  EXPECT_TRUE(IsProperSubRange(range[4], range[5]));

  EXPECT_FALSE(IsProperSubRange(range[5], range[0]));
  EXPECT_FALSE(IsProperSubRange(range[5], range[1]));
  EXPECT_FALSE(IsProperSubRange(range[5], range[2]));
  EXPECT_FALSE(IsProperSubRange(range[5], range[3]));
  EXPECT_FALSE(IsProperSubRange(range[5], range[4]));
}

TEST(IPRangeTest, TruncateIPAddress) {
  // Basic truncation.
  EXPECT_EQ(StringToIPAddressOrDie("192.0.2.0"),
            TruncateIPAddress(StringToIPAddressOrDie("192.0.2.1"), 24));
  EXPECT_EQ(StringToIPAddressOrDie("2001:db8::"),
            TruncateIPAddress(StringToIPAddressOrDie("2001:db8:f00::1"), 32));

  // Large lengths are okay.
  EXPECT_EQ(StringToIPAddressOrDie("192.0.2.1"),
            TruncateIPAddress(StringToIPAddressOrDie("192.0.2.1"), 999));
  EXPECT_EQ(StringToIPAddressOrDie("2001:db8:f00::1"),
            TruncateIPAddress(StringToIPAddressOrDie("2001:db8:f00::1"), 999));

  // The length parameter doesn't do anything surprising.
  int length = 999;
  EXPECT_EQ(StringToIPAddressOrDie("192.0.2.1"),
            TruncateIPAddress(StringToIPAddressOrDie("192.0.2.1"), length));
  EXPECT_EQ(999, length);

  // Negative lengths are prohibited.
  EXPECT_DEATH(TruncateIPAddress(StringToIPAddressOrDie("192.0.2.0"), -1),
               "length >= 0");
  EXPECT_DEATH(TruncateIPAddress(StringToIPAddressOrDie("2001:db8::"), -1),
               "length >= 0");

  // Empty addresses are prohibited.
  EXPECT_DEATH(TruncateIPAddress(IPAddress(), -1), "IsInitializedAddress");
  EXPECT_DEATH(TruncateIPAddress(IPAddress(), 24), "IsInitializedAddress");
}

TEST(IPRangeTest, Truncation) {
  {
    IPAddress addr;
    ASSERT_TRUE(StringToIPAddress("129.240.2.3", &addr));
    EXPECT_EQ("0.0.0.0/0",
              TruncatedAddressToIPRange(addr, 0).ToString());
    EXPECT_EQ("129.192.0.0/10",
              TruncatedAddressToIPRange(addr, 10).ToString());
    EXPECT_EQ("129.240.2.3/32",
              TruncatedAddressToIPRange(addr, 32).ToString());
  }

  {
    IPAddress addr;
    ASSERT_TRUE(StringToIPAddress("8001:700:300:1800::1", &addr));
    EXPECT_EQ("::/0",
              TruncatedAddressToIPRange(addr, 0).ToString());
    EXPECT_EQ("8001:700:300::/48",
              TruncatedAddressToIPRange(addr, 48).ToString());
    EXPECT_EQ("8001:700:300:1800::1/128",
              TruncatedAddressToIPRange(addr, 128).ToString());
  }

  {
    IPAddress addr;
    ASSERT_TRUE(StringToIPAddress("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
                                  &addr));
    EXPECT_EQ("::/0",
              TruncatedAddressToIPRange(addr, 0).ToString());
    EXPECT_EQ("8000::/1",
              TruncatedAddressToIPRange(addr, 1).ToString());

    EXPECT_EQ("ffff:fffe::/31",
              TruncatedAddressToIPRange(addr, 31).ToString());
    EXPECT_EQ("ffff:ffff::/32",
              TruncatedAddressToIPRange(addr, 32).ToString());
    EXPECT_EQ("ffff:ffff:8000::/33",
              TruncatedAddressToIPRange(addr, 33).ToString());


    EXPECT_EQ("ffff:ffff:ffff:fffe::/63",
              TruncatedAddressToIPRange(addr, 63).ToString());
    EXPECT_EQ("ffff:ffff:ffff:ffff::/64",
              TruncatedAddressToIPRange(addr, 64).ToString());
    EXPECT_EQ("ffff:ffff:ffff:ffff:8000::/65",
              TruncatedAddressToIPRange(addr, 65).ToString());

    EXPECT_EQ("ffff:ffff:ffff:ffff:ffff:fffe::/95",
              TruncatedAddressToIPRange(addr, 95).ToString());
    EXPECT_EQ("ffff:ffff:ffff:ffff:ffff:ffff::/96",
              TruncatedAddressToIPRange(addr, 96).ToString());
    EXPECT_EQ("ffff:ffff:ffff:ffff:ffff:ffff:8000:0/97",
              TruncatedAddressToIPRange(addr, 97).ToString());

    EXPECT_EQ("ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffe/127",
              TruncatedAddressToIPRange(addr, 127).ToString());
    EXPECT_EQ("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128",
              TruncatedAddressToIPRange(addr, 128).ToString());
  }

  {
    IPAddress addr;
    ASSERT_TRUE(StringToIPAddress("2001:4860:ffff::", &addr));
    EXPECT_EQ("2001:4860:f000::/36",
              TruncatedAddressToIPRange(addr, 36).ToString());
  }

  {
    IPAddress addr;
    ASSERT_TRUE(StringToIPAddress("127.0.0.1", &addr));
    EXPECT_EQ("127.0.0.1/32", TruncatedAddressToIPRange(addr, 33).ToString());
  }

  {
    IPAddress addr;
    ASSERT_TRUE(StringToIPAddress("::1", &addr));
    EXPECT_EQ("::1/128", TruncatedAddressToIPRange(addr, 129).ToString());
  }

  {
    const IPRange truncated = TruncatedAddressToIPRange(IPAddress(), -1234);
    EXPECT_EQ(IPRange(), truncated);
    EXPECT_EQ(IPAddress(), truncated.host());
    EXPECT_EQ(-1, truncated.length());
  }

  // MxN test of various bit positions and prefix lengths.
  {
    for (int bit = 0; bit < 128; bit++) {
      const IPAddress addr =
          UInt128ToIPAddress(absl::uint128(1) << (127 - bit));
      EXPECT_NE(IPAddress::Any6(), addr);
      for (int len = std::max(0, bit - 5); len <= std::min(128, bit + 5);
           len++) {
        const IPAddress truncated = TruncatedAddressToIPRange(addr, len).host();
        if (bit < len) {
          EXPECT_EQ(addr, truncated);
        } else {
          EXPECT_EQ(IPAddress::Any6(), truncated);
        }
      }
    }
    for (int bit = 0; bit < 32; bit++) {
      const IPAddress addr = HostUInt32ToIPAddress(1U << (31 - bit));
      EXPECT_NE(IPAddress::Any4(), addr);
      for (int len = 0; len <= 32; len++) {
        const IPAddress truncated = TruncatedAddressToIPRange(addr, len).host();
        if (bit < len) {
          EXPECT_EQ(addr, truncated);
        } else {
          EXPECT_EQ(IPAddress::Any4(), truncated);
        }
      }
    }
  }
}

// IPRange tests for ToPackedString() and PackedStringToIPRange().
TEST(IPRangeTest, PacksEmptyRange) {
  EXPECT_DEATH(IPRange().ToPackedString(), "Uninitialized address");
  IPRange result;
  EXPECT_FALSE(PackedStringToIPRange("", &result));
}

// This test takes a sample IPv4 and IPv6 address, and for each mask length,
// generates an IPRange and a truncated IPRange, then packs and unpacks these
// to verify that the truncated IPRange is reconstructed in both cases.
TEST(IPRangeTest, PacksIPv4AndIPv6Range) {
  std::string ips[] = {"172.16.255.47",
                       "1.2.3.4",
                       "0.0.0.0",
                       "0.0.1.0",
                       "0.1.0.1",
                       "1234:5678:aaaa:bbbb:cccc:dddd:eeee:ffff",
                       "2001:dead::1",
                       "2001::1",
                       "2001::",
                       "::1",
                       "0::",
                       "127.0.0.1",
                       "2001:dead:beaf::1",
                       "2001:dead::"};
  for (size_t i = 0; i < ARRAYSIZE(ips); ++i) {
    IPAddress ip = StringToIPAddressOrDie(ips[i]);
    int max_subnet_length = (ip.address_family() == AF_INET ? 32 : 128);
    std::string packed;
    IPRange unpacked;
    for (int subnet_length = 0;
         subnet_length <= max_subnet_length;
         ++subnet_length) {
      const IPRange truncated = TruncatedAddressToIPRange(ip, subnet_length);
      packed = truncated.ToPackedString();
      ASSERT_TRUE(PackedStringToIPRange(packed, &unpacked));
      EXPECT_EQ(truncated, unpacked);

      // We expect the result from unpacking to be the original IPRange but
      // truncated.
      const IPRange original = IPRange(ip, subnet_length);
      packed = original.ToPackedString();
      ASSERT_TRUE(PackedStringToIPRange(packed, &unpacked));
      EXPECT_EQ(truncated, unpacked);
    }
  }
}

TEST(IPRangeTest, VerifyPackedStringFormat) {
  std::string ipranges[] = {"0.0.0.0/0", "::/0"};
  std::string expected_packed[] = {string("\xc8", 1), std::string("\x00", 1)};
  for (size_t i = 0; i < ARRAYSIZE(ipranges); ++i) {
    IPRange iprange = StringToIPRangeOrDie(ipranges[i]);
    std::string packed;
    IPRange unpacked;
    packed = iprange.ToPackedString();
    EXPECT_EQ(expected_packed[i], packed);
    ASSERT_TRUE(PackedStringToIPRange(packed, &unpacked));
    EXPECT_EQ(iprange, unpacked);
  }
}

TEST(IPRangeTest, AcceptsNull) {
  IPAddress kIpv6(
      StringToIPAddressOrDie("8888:9999:1234:abcd:cdef:efab:ab12:1012"));
  const IPRange original = TruncatedAddressToIPRange(kIpv6, 27);
  const std::string packed = original.ToPackedString();
  EXPECT_TRUE(PackedStringToIPRange(packed, NULL));
  EXPECT_FALSE(PackedStringToIPRange(string(), NULL));
}

TEST(IPRangeTest, FailsOnBadHeaderLengths) {
  IPAddress kIpv6(
      StringToIPAddressOrDie("1111:2222:3333:4444:5555:6666:7777:8888"));
  const IPRange original = TruncatedAddressToIPRange(kIpv6, 52);
  const std::string packed = original.ToPackedString();
  int bad_lengths[] = {129, 199, 233, 255, -1, 256, 1000};
  for (size_t i = 0; i < ARRAYSIZE(bad_lengths); ++i) {
    IPRange result;
    std::string bad_packed = static_cast<char>(bad_lengths[i]) + packed;
    EXPECT_FALSE(PackedStringToIPRange(bad_packed, &result));
  }
}

TEST(IPRangeTest, FailsOnBadStringLengths) {
  IPAddress kIpv6(
      StringToIPAddressOrDie("8888:9999:aaaa:bbbb:cccc:dddd:eeee:ffff"));
  const IPRange original = TruncatedAddressToIPRange(kIpv6, 52);
  std::string packed = original.ToPackedString();
  IPRange result;
  EXPECT_TRUE(PackedStringToIPRange(packed, &result));
  packed.push_back('x');
  EXPECT_FALSE(PackedStringToIPRange(packed, &result));
}

TEST(IPRangeTest, InvalidPackedStringConversion) {
  IPRange ip_range;
  // Invalid conversion.
  EXPECT_FALSE(PackedStringToIPRange("something very bad", &ip_range));
  // Valid conversion.
  const std::string packed = StringToIPRangeOrDie("1.0.0.0/8").ToPackedString();
  ASSERT_TRUE(PackedStringToIPRange(packed, &ip_range));
  EXPECT_EQ(ip_range.ToString(), "1.0.0.0/8");
}

TEST(IPAddressPlusNTest, AddZeroDoesNotChangeIPv4) {
  IPAddress addr = StringToIPAddressOrDie("10.1.1.150");
  IPAddress result;
  EXPECT_TRUE(IPAddressPlusN(addr, 0, &result));
  IPAddress expected_result = StringToIPAddressOrDie("10.1.1.150");
  EXPECT_EQ(expected_result, result);
}

TEST(IPAddressPlusNTest, AddOneToIPv4) {
  IPAddress addr = StringToIPAddressOrDie("10.1.1.150");
  IPAddress result;
  EXPECT_TRUE(IPAddressPlusN(addr, 1, &result));
  IPAddress expected_result = StringToIPAddressOrDie("10.1.1.151");
  EXPECT_EQ(expected_result, result);

  // Also test override cases.
  EXPECT_TRUE(IPAddressPlusN(addr, 1, &addr));
  EXPECT_EQ(expected_result, addr);
}

TEST(IPAddressPlusNTest, AddToIPv4CrossesLastOctetBoundary) {
  IPAddress addr = StringToIPAddressOrDie("10.1.1.150");
  IPAddress result;
  EXPECT_TRUE(IPAddressPlusN(addr, 150, &result));
  IPAddress expected_result = StringToIPAddressOrDie("10.1.2.44");
  EXPECT_EQ(expected_result, result);
}

TEST(IPAddressPlusNTest, SubtractFromIPv4) {
  IPAddress addr = StringToIPAddressOrDie("10.1.1.1");

  IPAddress result;
  EXPECT_TRUE(IPAddressPlusN(addr, -1, &result));
  IPAddress expected_result = StringToIPAddressOrDie("10.1.1.0");
  EXPECT_EQ(expected_result, result);

  EXPECT_TRUE(IPAddressPlusN(addr, -2, &result));
  expected_result = StringToIPAddressOrDie("10.1.0.255");
  EXPECT_EQ(expected_result, result);
}

TEST(IPAddressPlusNTest, AddToIPv6) {
  IPAddress addr = StringToIPAddressOrDie("8002:12::aab0");
  IPAddress result;
  EXPECT_TRUE(IPAddressPlusN(addr, 15, &result));
  IPAddress expected_result = StringToIPAddressOrDie("8002:12::aabf");
  EXPECT_EQ(expected_result, result);
}

TEST(IPAddressPlusNTest, SubtractFromIPv6) {
  IPAddress addr = StringToIPAddressOrDie("8002:12::aab0");
  IPAddress result;
  EXPECT_TRUE(IPAddressPlusN(addr, -0xaab1, &result));
  IPAddress expected_result =
      StringToIPAddressOrDie("8002:11:ffff:ffff:ffff:ffff:ffff:ffff");
  EXPECT_EQ(expected_result, result);
}

TEST(IPAddressPlusNTest, AddCrossesIPv4SpaceBoundary) {
  IPAddress addr = StringToIPAddressOrDie("192.0.0.0");

  IPAddress result;
  EXPECT_TRUE(IPAddressPlusN(addr, 0x3fffffff, &result));
  IPAddress expected_result = StringToIPAddressOrDie("255.255.255.255");
  EXPECT_EQ(expected_result, result);

  EXPECT_FALSE(IPAddressPlusN(addr, 0x40000000, &result));
}

TEST(IPAddressPlusNTest, SubtractCrossesIPv4SpaceBoundary) {
  IPAddress addr = StringToIPAddressOrDie("4.0.0.0");

  IPAddress result;
  EXPECT_TRUE(IPAddressPlusN(addr, -0x4000000, &result));
  IPAddress expected_result = StringToIPAddressOrDie("0.0.0.0");
  EXPECT_EQ(expected_result, result);

  EXPECT_FALSE(IPAddressPlusN(addr, -0x4000001, &result));
}

TEST(IPAddressPlusNDeathTest, InvalidAddressFamily) {
  IPAddress uninit_addr, result_addr;
  bool result = true;
  EXPECT_DEATH(result = IPAddressPlusN(uninit_addr, 1, &result_addr),
               "Invalid address family");
  EXPECT_TRUE(result || !result);
}

TEST(IPRangeTest, Subtract) {
  {
    IPRange range, sub_range;
    ASSERT_TRUE(StringToIPRange("0.0.0.0/0", &range));
    ASSERT_TRUE(StringToIPRange("10.0.0.0/7", &sub_range));

    std::vector<IPRange> diff_range;
    EXPECT_TRUE(SubtractIPRange(range, sub_range, &diff_range));
    ASSERT_EQ(7u, diff_range.size());
    EXPECT_EQ("8.0.0.0/7", diff_range[0].ToString());
    EXPECT_EQ("12.0.0.0/6", diff_range[1].ToString());
    EXPECT_EQ("0.0.0.0/5", diff_range[2].ToString());
    EXPECT_EQ("16.0.0.0/4", diff_range[3].ToString());
    EXPECT_EQ("32.0.0.0/3", diff_range[4].ToString());
    EXPECT_EQ("64.0.0.0/2", diff_range[5].ToString());
    EXPECT_EQ("128.0.0.0/1", diff_range[6].ToString());
  }

  {
    const IPRange range = StringToIPRangeOrDie("0.0.0.0/0");
    const IPRange sub_range = StringToIPRangeOrDie("0.0.0.0/1");

    std::vector<IPRange> diff_range;
    EXPECT_TRUE(SubtractIPRange(range, sub_range, &diff_range));
    ASSERT_EQ(1u, diff_range.size());
    EXPECT_EQ("128.0.0.0/1", diff_range[0].ToString());
  }

  {
    IPRange range, sub_range;
    ASSERT_TRUE(StringToIPRange("8002::/15", &range));
    ASSERT_TRUE(StringToIPRange("8003:aaa0::/28", &sub_range));

    std::vector<IPRange> diff_range;
    EXPECT_TRUE(SubtractIPRange(range, sub_range, &diff_range));
    ASSERT_EQ(13u, diff_range.size());
    EXPECT_EQ("8003:aab0::/28", diff_range[0].ToString());
    EXPECT_EQ("8003:aa80::/27", diff_range[1].ToString());
    EXPECT_EQ("8003:aac0::/26", diff_range[2].ToString());
    EXPECT_EQ("8003:aa00::/25", diff_range[3].ToString());
    EXPECT_EQ("8003:ab00::/24", diff_range[4].ToString());
    EXPECT_EQ("8003:a800::/23", diff_range[5].ToString());
    EXPECT_EQ("8003:ac00::/22", diff_range[6].ToString());
    EXPECT_EQ("8003:a000::/21", diff_range[7].ToString());
    EXPECT_EQ("8003:b000::/20", diff_range[8].ToString());
    EXPECT_EQ("8003:8000::/19", diff_range[9].ToString());
    EXPECT_EQ("8003:c000::/18", diff_range[10].ToString());
    EXPECT_EQ("8003::/17", diff_range[11].ToString());
    EXPECT_EQ("8002::/16", diff_range[12].ToString());
  }

  {
    IPRange range, sub_range;
    ASSERT_TRUE(StringToIPRange("::0/0", &range));
    ASSERT_TRUE(StringToIPRange("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128",
                                &sub_range));

    std::vector<IPRange> diff_range;
    EXPECT_TRUE(SubtractIPRange(range, sub_range, &diff_range));
    ASSERT_EQ(128u, diff_range.size());
    EXPECT_EQ("ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffe/128",
              diff_range[0].ToString());
    EXPECT_EQ("ffff:ffff:fffe::/48", diff_range[80].ToString());
    EXPECT_EQ("::/1", diff_range[127].ToString());
  }

  {
    IPRange range, sub_range;
    ASSERT_TRUE(StringToIPRange("10.0.0.0/7", &range));
    ASSERT_TRUE(StringToIPRange("12.1.0.0/16", &sub_range));

    std::vector<IPRange> diff_range;
    // Return false if not a sub-range.
    EXPECT_FALSE(SubtractIPRange(range, sub_range, &diff_range));
    EXPECT_TRUE(diff_range.empty());
  }

  {
    IPRange range, sub_range;
    ASSERT_TRUE(StringToIPRange("10.0.0.0/7", &range));
    ASSERT_TRUE(StringToIPRange("ab0::/16", &sub_range));

    std::vector<IPRange> diff_range;
    // Return false if not a sub-range.
    EXPECT_FALSE(SubtractIPRange(range, sub_range, &diff_range));
    EXPECT_TRUE(diff_range.empty());
  }
}

TEST(IPRangeTest, Ordering) {
  const std::string kIPString1 = "1.2.3.4";
  const std::string kIPString2 = "4.3.2.1";
  const std::string kIPString3 = "2001:db8::";
  const std::string kIPString4 = "3ffe::";

  IPAddress addr1, addr2, addr3, addr4;

  ASSERT_TRUE(StringToIPAddress(kIPString1, &addr1));
  ASSERT_TRUE(StringToIPAddress(kIPString2, &addr2));
  ASSERT_TRUE(StringToIPAddress(kIPString3, &addr3));
  ASSERT_TRUE(StringToIPAddress(kIPString4, &addr4));

  IPRange range0;
  IPRange range1_1(addr1, 8);
  IPRange range1_2(addr1, 16);
  IPRange range1_3(addr1, 24);
  IPRange range2_1(addr2, 8);
  IPRange range2_2(addr2, 16);
  IPRange range2_3(addr2, 24);
  IPRange range3(addr3, 32);
  IPRange range4(addr4, 16);

  std::set<IPRange, IPRangeOrdering> ranges;
  ranges.insert(range4);
  ranges.insert(range3);
  ranges.insert(range3);
  ranges.insert(range2_3);
  ranges.insert(range2_2);
  ranges.insert(range2_1);
  ranges.insert(range2_1);
  ranges.insert(range0);
  ranges.insert(range1_3);
  ranges.insert(range1_2);
  ranges.insert(range1_1);
  ranges.insert(range1_1);

  EXPECT_EQ(9u, ranges.size());

  std::vector<IPRange> sorted_ranges(ranges.begin(), ranges.end());
  ASSERT_EQ(9u, sorted_ranges.size());
  EXPECT_EQ(range0, sorted_ranges[0]);
  EXPECT_EQ(range1_1, sorted_ranges[1]);
  EXPECT_EQ(range1_2, sorted_ranges[2]);
  EXPECT_EQ(range1_3, sorted_ranges[3]);
  EXPECT_EQ(range2_1, sorted_ranges[4]);
  EXPECT_EQ(range2_2, sorted_ranges[5]);
  EXPECT_EQ(range2_3, sorted_ranges[6]);
  EXPECT_EQ(range3, sorted_ranges[7]);
  EXPECT_EQ(range4, sorted_ranges[8]);
}

TEST(IPRangeTest, Hash) {
  const std::string kIPString1 = "1.2.3.4";
  const std::string kIPString2 = "4.3.2.1";
  const std::string kIPString3 = "2001:db8::";
  const std::string kIPString4 = "3ffe::";

  IPAddress addr1, addr2, addr3, addr4;

  ASSERT_TRUE(StringToIPAddress(kIPString1, &addr1));
  ASSERT_TRUE(StringToIPAddress(kIPString2, &addr2));
  ASSERT_TRUE(StringToIPAddress(kIPString3, &addr3));
  ASSERT_TRUE(StringToIPAddress(kIPString4, &addr4));

  IPRange range0;
  IPRange range1_1(addr1, 8);
  IPRange range1_2(addr1, 16);
  IPRange range1_3(addr1, 24);
  IPRange range2_1(addr2, 8);
  IPRange range2_2(addr2, 16);
  IPRange range2_3(addr2, 24);
  IPRange range3(addr3, 32);
  IPRange range4(addr4, 16);

  hash_set<IPRange> range_map;
  range_map.insert(range4);
  range_map.insert(range3);
  range_map.insert(range3);
  range_map.insert(range2_3);
  range_map.insert(range2_2);
  range_map.insert(range2_1);
  range_map.insert(range2_1);
  range_map.insert(range1_3);
  range_map.insert(range1_2);
  range_map.insert(range1_1);
  range_map.insert(range1_1);
  range_map.insert(range0);
  range_map.insert(IPRange());

  EXPECT_EQ(9u, range_map.size());
  EXPECT_EQ(1u, range_map.count(range0));
  EXPECT_EQ(1u, range_map.count(range1_1));
  EXPECT_EQ(1u, range_map.count(range1_2));
  EXPECT_EQ(1u, range_map.count(range1_3));
  EXPECT_EQ(1u, range_map.count(range2_1));
  EXPECT_EQ(1u, range_map.count(range2_2));
  EXPECT_EQ(1u, range_map.count(range2_3));
  EXPECT_EQ(1u, range_map.count(range3));
  EXPECT_EQ(1u, range_map.count(range4));
}

TEST(IPRangeTest, IsInitializedRange) {
  IPRange uninit_range;
  EXPECT_FALSE(IsInitializedRange(uninit_range));

  IPAddress addr4;
  ASSERT_TRUE(StringToIPAddress("129.224.0.0", &addr4));
  IPRange invalid_range4(addr4, 10);
  EXPECT_TRUE(IsInitializedRange(invalid_range4));

  ASSERT_TRUE(StringToIPAddress("129.192.0.0", &addr4));
  IPRange valid_range4(addr4, 10);
  EXPECT_TRUE(IsInitializedRange(valid_range4));

  IPAddress addr6;
  ASSERT_TRUE(StringToIPAddress("8001:700:300::", &addr6));
  IPRange invalid_range6(addr6, 39);
  EXPECT_TRUE(IsInitializedRange(invalid_range6));

  IPRange valid_range6(addr6, 40);
  EXPECT_TRUE(IsInitializedRange(valid_range6));
}

TEST(IPRangeTest, UnsafeConstruct) {
  // Valid inputs.
  IPRange::UnsafeConstruct(IPAddress(), -1);
  IPRange::UnsafeConstruct(StringToIPAddressOrDie("192.0.2.0"), 24);
  IPRange::UnsafeConstruct(StringToIPAddressOrDie("2001:db8::"), 32);

  // Invalid inputs fail only in debug mode.
  EXPECT_DEATH(IPRange::UnsafeConstruct(IPAddress(), -2),
               "Length is inconsistent with address family");
  EXPECT_DEATH(
      IPRange::UnsafeConstruct(StringToIPAddressOrDie("192.0.2.1"), 33),
      "Length is inconsistent with address family");
  EXPECT_DEATH(
      IPRange::UnsafeConstruct(StringToIPAddressOrDie("2001:db8::1"), 129),
      "Length is inconsistent with address family");
  EXPECT_DEATH(
      IPRange::UnsafeConstruct(StringToIPAddressOrDie("192.0.2.1"), 24),
      "Host has bits set beyond the prefix length");
  EXPECT_DEATH(
      IPRange::UnsafeConstruct(StringToIPAddressOrDie("2001:db8::1"), 32),
      "Host has bits set beyond the prefix length");
  EXPECT_DEATH(
      IPRange::UnsafeConstruct(StringToIPAddressOrDie("192.0.2.0"), -1),
      "length >= 0");
  EXPECT_DEATH(
      IPRange::UnsafeConstruct(StringToIPAddressOrDie("2001:db8::"), -1),
      "length >= 0");
}

TEST(IPRangeTest, IsValidRange) {
  IPRange uninit_range;
  EXPECT_FALSE(IsValidRange(uninit_range));

  IPAddress addr4;
  ASSERT_TRUE(StringToIPAddress("129.192.0.0", &addr4));
  IPRange valid_range4(addr4, 10);
  EXPECT_TRUE(IsValidRange(valid_range4));

  IPAddress addr6;
  ASSERT_TRUE(StringToIPAddress("8001:700:300::", &addr6));
  IPRange valid_range6(addr6, 40);
  EXPECT_TRUE(IsValidRange(valid_range6));
}

TEST(IPRangeTest, IPAddressIntervalToSubnets_UninitializedIPAddresses) {
  IPAddress first_addr, last_addr;
  std::vector<IPRange> covering_subnets;
  EXPECT_FALSE(IPAddressIntervalToSubnets(first_addr, last_addr,
                                          &covering_subnets));
  EXPECT_TRUE(covering_subnets.empty());
}

TEST(IPRangeTest, IPAddressIntervalToSubnets_AddressFamilyMismatch) {
  IPAddress first_addr = StringToIPAddressOrDie("4.1.0.1");
  IPAddress last_addr = StringToIPAddressOrDie("8001:700:300::11");
  std::vector<IPRange> covering_subnets;
  EXPECT_FALSE(IPAddressIntervalToSubnets(first_addr, last_addr,
                                          &covering_subnets));
  EXPECT_TRUE(covering_subnets.empty());
}

TEST(IPRangeTest, IPAddressIntervalToSubnets_InvalidInterval) {
  IPAddress first_addr = StringToIPAddressOrDie("4.1.0.1");
  IPAddress last_addr = StringToIPAddressOrDie("4.1.0.0");
  std::vector<IPRange> covering_subnets;
  EXPECT_FALSE(IPAddressIntervalToSubnets(first_addr, last_addr,
                                          &covering_subnets));
  EXPECT_TRUE(covering_subnets.empty());
}

TEST(IPRangeTest, IPAddressIntervalToSubnets_SingleAddressInterval) {
  IPAddress first_addr = StringToIPAddressOrDie("4.1.0.1");
  IPAddress last_addr = first_addr;
  std::vector<IPRange> covering_subnets;
  EXPECT_TRUE(IPAddressIntervalToSubnets(first_addr, last_addr,
                                         &covering_subnets));
  ASSERT_EQ(1u, covering_subnets.size());
  EXPECT_EQ(IPRange(first_addr), covering_subnets[0]);
}

TEST(IPRangeTest, IPAddressIntervalToSubnets_MaxIPv4Interval) {
  IPAddress first_addr = StringToIPAddressOrDie("0.0.0.0");
  IPAddress last_addr = StringToIPAddressOrDie("255.255.255.255");
  std::vector<IPRange> covering_subnets;
  EXPECT_TRUE(IPAddressIntervalToSubnets(first_addr, last_addr,
                                         &covering_subnets));
  ASSERT_EQ(1u, covering_subnets.size());
  EXPECT_EQ(StringToIPRangeOrDie("0.0.0.0/0"), covering_subnets[0]);
}

TEST(IPRangeTest, IPAddressIntervalToSubnets_MaxIPv6Interval) {
  IPAddress first_addr = StringToIPAddressOrDie("::0");
  IPAddress last_addr =
      StringToIPAddressOrDie("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
  std::vector<IPRange> covering_subnets;
  EXPECT_TRUE(IPAddressIntervalToSubnets(first_addr, last_addr,
                                         &covering_subnets));
  ASSERT_EQ(1u, covering_subnets.size());
  EXPECT_EQ(StringToIPRangeOrDie("::0/0"), covering_subnets[0]);
}

TEST(IPRangeTest, IPAddressIntervalToSubnets_TestIPv4_Case1) {
  IPAddress first_addr = StringToIPAddressOrDie("255.255.254.0");
  IPAddress last_addr = StringToIPAddressOrDie("255.255.255.255");

  std::vector<IPRange> covering_subnets;
  EXPECT_TRUE(IPAddressIntervalToSubnets(first_addr, last_addr,
                                         &covering_subnets));
  ASSERT_EQ(1u, covering_subnets.size());
  EXPECT_EQ(StringToIPRangeOrDie("255.255.254.0/23"), covering_subnets[0]);
}

#if HAVE_VECTOR_CHECKEQ
// No EXPECT_EQ on vectors in depot3
TEST(IPRangeTest, IPAddressIntervalToSubnets_TestIPv4_Case2) {
  IPAddress first_addr = StringToIPAddressOrDie("4.191.0.0");
  IPAddress last_addr = StringToIPAddressOrDie("6.1.0.255");

  std::vector<IPRange> expected_covering_subnets;
  expected_covering_subnets.push_back(StringToIPRangeOrDie("4.191.0.0/16"));
  expected_covering_subnets.push_back(StringToIPRangeOrDie("4.192.0.0/10"));
  expected_covering_subnets.push_back(StringToIPRangeOrDie("5.0.0.0/8"));
  expected_covering_subnets.push_back(StringToIPRangeOrDie("6.0.0.0/16"));
  expected_covering_subnets.push_back(StringToIPRangeOrDie("6.1.0.0/24"));

  std::vector<IPRange> covering_subnets;
  EXPECT_TRUE(IPAddressIntervalToSubnets(first_addr, last_addr,
                                         &covering_subnets));
  EXPECT_EQ(expected_covering_subnets, covering_subnets);
}
#endif

#if HAVE_VECTOR_CHECKEQ
// No EXPECT_EQ on vectors in depot3
TEST(IPRangeTest, IPAddressIntervalToSubnets_TestIPv6) {
  IPAddress first_addr = StringToIPAddressOrDie("2001:db8::");
  IPAddress last_addr = StringToIPAddressOrDie("2001:2000::");

  std::vector<IPRange> expected_covering_subnets;
  expected_covering_subnets.push_back(StringToIPRangeOrDie("2001:db8::/29"));
  expected_covering_subnets.push_back(StringToIPRangeOrDie("2001:dc0::/26"));
  expected_covering_subnets.push_back(StringToIPRangeOrDie("2001:e00::/23"));
  expected_covering_subnets.push_back(StringToIPRangeOrDie("2001:1000::/20"));
  expected_covering_subnets.push_back(StringToIPRangeOrDie("2001:2000::/128"));

  std::vector<IPRange> covering_subnets;
  EXPECT_TRUE(IPAddressIntervalToSubnets(first_addr, last_addr,
                                         &covering_subnets));
  EXPECT_EQ(expected_covering_subnets, covering_subnets);
}
#endif

TEST(IPRangeTest, IsRangeIndexValid) {
  IPAddress base_addr4 = StringToIPAddressOrDie("1.2.3.4");
  for (int length = 1; length <= 32; ++length) {
    IPRange range(base_addr4, length);
    absl::uint128 size1((absl::uint128(1) << (32 - length)) - 1);
    EXPECT_TRUE(IsRangeIndexValid(range, size1))
        << "length=" << length << " size1=" << size1;
    absl::uint128 size2(absl::uint128(1) << (32 - length));
    EXPECT_FALSE(IsRangeIndexValid(range, size2))
        << "length=" << length << " size2=" << size2;
  }

  IPAddress base_addr6 = StringToIPAddressOrDie("2001:db8::");
  for (int length = 1; length < 128; ++length) {
    IPRange range(base_addr6, length);
    absl::uint128 size1((absl::uint128(1) << (128 - length)) - 1);
    EXPECT_TRUE(IsRangeIndexValid(range, size1))
        << "length=" << length << " size1=" << size1;
    absl::uint128 size2(absl::uint128(1) << (128 - length));
    EXPECT_FALSE(IsRangeIndexValid(range, size2))
        << "length=" << length << " size2=" << size2;
  }
  // 1 << 128 doesn't fit into a uint128, so use a different test
  // when length = 0.
  IPRange range(base_addr6, 0);
  EXPECT_TRUE(IsRangeIndexValid(range, kuint128max));
}

TEST(IPRangeTest, NthAddressInRange) {
  IPRange range;

  ASSERT_TRUE(StringToIPRange("1.2.3.4/32", &range));
  EXPECT_EQ("1.2.3.4", NthAddressInRange(range, 0).ToString());

  ASSERT_TRUE(StringToIPRange("1.2.3.0/24", &range));
  EXPECT_EQ("1.2.3.0", NthAddressInRange(range, 0).ToString());
  EXPECT_EQ("1.2.3.255", NthAddressInRange(range, 255).ToString());

  ASSERT_TRUE(StringToIPRange("0.0.0.0/0", &range));
  EXPECT_EQ("0.0.255.255", NthAddressInRange(range, 0xffff).ToString());
  EXPECT_EQ("255.255.255.255", NthAddressInRange(range, kuint32max).ToString());

  ASSERT_TRUE(StringToIPRange("fedc:ba98:7654:3210:123:4567:89ab:cdef/128",
                              &range));
  EXPECT_EQ("fedc:ba98:7654:3210:123:4567:89ab:cdef",
            NthAddressInRange(range, 0).ToString());

  ASSERT_TRUE(StringToIPRange("fedc:ba98:7654:3210:123::/80",
                              &range));
  EXPECT_EQ("fedc:ba98:7654:3210:123::f",
            NthAddressInRange(range, 15).ToString());
  EXPECT_EQ("fedc:ba98:7654:3210:123:0:ffff:ffff",
            NthAddressInRange(range, kuint32max).ToString());

  ASSERT_TRUE(StringToIPRange("::/0", &range));
  EXPECT_EQ("::0.1.0.0", NthAddressInRange(range, 0x10000).ToString());
  EXPECT_EQ("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
            NthAddressInRange(range, kuint128max).ToString());
}

TEST(IPAddress, IndexInRange) {
  EXPECT_EQ(0, IndexInRange(StringToIPRangeOrDie("1.1.1.0/24"),
                            StringToIPAddressOrDie("1.1.1.0")));
  EXPECT_EQ(200, IndexInRange(StringToIPRangeOrDie("1.1.1.0/24"),
                              StringToIPAddressOrDie("1.1.1.200")));
  EXPECT_EQ(266, IndexInRange(StringToIPRangeOrDie("192.1.192.0/22"),
                              StringToIPAddressOrDie("192.1.193.10")));
  EXPECT_EQ(8, IndexInRange(StringToIPRangeOrDie("1.1.1.240/28"),
                            StringToIPAddressOrDie("1.1.1.248")));
  EXPECT_EQ(1, IndexInRange(IPRange(StringToIPAddressOrDie("1.1.1.1"), 24),
                            StringToIPAddressOrDie("1.1.1.1")));

  EXPECT_EQ(128, IndexInRange(
      StringToIPRangeOrDie("2001:718:1001:700:200:5efe:c0a8:0300/120"),
      StringToIPAddressOrDie("2001:718:1001:700:200:5efe:c0a8:0380")));
  EXPECT_EQ(286326784, IndexInRange(
      StringToIPRangeOrDie("2001:718:1001:700:0000:0000:0000:0000/64"),
      StringToIPAddressOrDie("2001:718:1001:700:0000:0000:1111:0000")));
  EXPECT_EQ(16, IndexInRange(
      IPRange(StringToIPAddressOrDie("0:0:0:0:0:0:8:1"), 120),
      StringToIPAddressOrDie("0:0:0:0:0:0:8:10")));

  EXPECT_DEATH(IndexInRange(StringToIPRangeOrDie("1.1.1.0/24"),
                            StringToIPAddressOrDie("1.1.2.0")),
                            "is not within");
  EXPECT_DEATH(IndexInRange(
      StringToIPRangeOrDie("2001:718:1001:700:200:5efe:c0a8:0300/120"),
      StringToIPAddressOrDie("3001:718:1001:700:200:5efe:c0a8:0380")),
      "is not within");

  EXPECT_DEATH(IndexInRange(
      StringToIPRangeOrDie("0:0:0:0:0:0:c0a8:0/120"),
      StringToIPAddressOrDie("192.168.0.10")),
      "is not within");
  EXPECT_DEATH(IndexInRange(
      StringToIPRangeOrDie("192.168.0.0/24"),
      StringToIPAddressOrDie("0:0:0:0:0:0:c0a8:000a")),
      "is not within");
}

TEST(IPRangeTest, LoggingUninitialized) {
  std::ostringstream out;
  out << IPRange();
  EXPECT_EQ("<uninitialized IPRange>", out.str());
}

TEST(IPRangeDeathTest, MiscUninitialized) {
  EXPECT_EQ(IPAddress(), IPRange().host());
  EXPECT_DEATH(IPRange().network_address(), "Unknown address family");
  EXPECT_DEATH(IPRange().broadcast_address(), "Unknown address family");

  // This constructor is quite strange, but some callers use it.
  const IPRange bad_range(IPAddress(), 0);
  EXPECT_DEATH(bad_range.network_address(), "Unknown address family");
}

// Invalid conversion in *OrDie() functions.
TEST(IPRangeDeathTest, InvalidStringConversion) {
  // Invalid conversions.
  EXPECT_DEATH(StringToIPRangeOrDie("foo/10"),
               "Invalid IP range foo/10");
  EXPECT_DEATH(StringToIPRangeOrDie(string("128.59.16.20/16")),
               "Invalid IP range");
  EXPECT_DEATH(StringToIPRangeOrDie("::g/42"),
               "Invalid IP range ::g/42");
  EXPECT_DEATH(StringToIPRangeOrDie("2001:db8:1234::/32"),
               "Invalid IP range 2001:db8:1234::/32");

  EXPECT_DEATH(StringToIPRangeAndTruncateOrDie("foo/10"),
               "Invalid IP range foo/10");
  EXPECT_DEATH(StringToIPRangeAndTruncateOrDie(string("128.59.16.320/16")),
               "Invalid IP range 128.59.16.320/16");
  EXPECT_DEATH(StringToIPRangeAndTruncateOrDie("::g/42"),
               "Invalid IP range ::g/42");
  EXPECT_DEATH(StringToIPRangeAndTruncateOrDie("2001:db8:1234::/132"),
               "Invalid IP range 2001:db8:1234::/132");

  // Valid conversions.
  EXPECT_EQ(StringToIPRangeOrDie("192.168.253.0/24").ToString(),
            "192.168.253.0/24");
  EXPECT_EQ(StringToIPRangeOrDie("2001:db8:1234::/48").ToString(),
            "2001:db8:1234::/48");
  EXPECT_EQ(StringToIPRangeAndTruncateOrDie("1.2.3.4/16").ToString(),
            "1.2.0.0/16");
  EXPECT_EQ(StringToIPRangeAndTruncateOrDie("2001:db8:1234::/32").ToString(),
            "2001:db8::/32");
}

TEST(IPRangeDeathTest, InvalidAddressFamily) {
  IPAddress ip;
  IPRange range;

  ASSERT_FALSE(IsInitializedAddress(ip));

  EXPECT_DEATH(range = IPRange(ip), "");
}

TEST(MaskLengthToIPAddress, InvalidConversions) {
  IPAddress result;
  EXPECT_FALSE(MaskLengthToIPAddress(AF_INET, -1, &result));
  EXPECT_FALSE(MaskLengthToIPAddress(AF_INET, 33, &result));
  EXPECT_FALSE(MaskLengthToIPAddress(AF_INET6, -1, &result));
  EXPECT_FALSE(MaskLengthToIPAddress(AF_INET6, 129, &result));
  EXPECT_FALSE(MaskLengthToIPAddress(AF_UNSPEC, 12, &result));
}

TEST(MaskLengthToIPAddress, IPv4Conversions) {
  const char* kValues[] = {
    "255.255.255.255",
    "255.255.255.254",
    "255.255.255.252",
    "255.255.255.248",
    "255.255.255.240",
    "255.255.255.224",
    "255.255.255.192",
    "255.255.255.128",
    "255.255.255.0",
    "255.255.254.0",
    "255.255.252.0",
    "255.255.248.0",
    "255.255.240.0",
    "255.255.224.0",
    "255.255.192.0",
    "255.255.128.0",
    "255.255.0.0",
    "255.254.0.0",
    "255.252.0.0",
    "255.248.0.0",
    "255.240.0.0",
    "255.224.0.0",
    "255.192.0.0",
    "255.128.0.0",
    "255.0.0.0",
    "254.0.0.0",
    "252.0.0.0",
    "248.0.0.0",
    "240.0.0.0",
    "224.0.0.0",
    "192.0.0.0",
    "128.0.0.0",
    "0.0.0.0"
  };

  for (size_t i = 0; i < ABSL_ARRAYSIZE(kValues); ++i) {
    IPAddress mask;
    EXPECT_TRUE(MaskLengthToIPAddress(AF_INET, 32 - i, &mask));
    EXPECT_EQ(kValues[i], mask.ToString()) << "Mask for " << 32 - i;
  }
}

TEST(MaskLengthToIPAddress, IPv6Conversions) {
  const struct MaskExpected {
    int length;
    const std::string expected;
  } kTests[] = {
    {0, "::"},
    {1, "8000::"},
    {15, "fffe::"},
    {31, "ffff:fffe::"},
    {47, "ffff:ffff:fffe::"},
    {59, "ffff:ffff:ffff:ffe0::"},
    {63, "ffff:ffff:ffff:fffe::"},
    {64, "ffff:ffff:ffff:ffff::"},
    {65, "ffff:ffff:ffff:ffff:8000::"},
    {79, "ffff:ffff:ffff:ffff:fffe::"},
    {95, "ffff:ffff:ffff:ffff:ffff:fffe::"},
    {111, "ffff:ffff:ffff:ffff:ffff:ffff:fffe:0"},
    {127, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffe"},
    {128, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"},
  };

  for (const MaskExpected& test : kTests) {
    IPAddress mask;
    EXPECT_TRUE(MaskLengthToIPAddress(AF_INET6, test.length, &mask));
    EXPECT_EQ(test.expected, mask.ToString()) << "Mask for " << test.length;
  }
}

TEST(NetMaskToMaskLength, Invalid) {
  IPAddress uninitialized;
  EXPECT_FALSE(NetMaskToMaskLength(uninitialized, nullptr));

  const char* kInvalid[] = {
    "127.0.0.0",
    "255.255.0.255",
    "255.254.255.255",
    "255.0.0.1",
    "ffff:ffff:7fff::",
    "7fff:ffff:ffff::",
    "ffff:ff7f:ffff::",
    "ffff:ffff:ffff:7fff::",
    "ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffd",
    "ffff:ffff:ffff:ffff:ffff:ffff:fffd::",
    "ffff:ffff:ffff:ffff:ffff:fffd::",
    "ffff:ffff:ffff:ffff:fffd::",
    "ffff:ffff:ffff:fffd::",
  };

  for (const std::string& mask : kInvalid) {
    EXPECT_FALSE(NetMaskToMaskLength(StringToIPAddressOrDie(mask), nullptr))
        << "Failed on " << mask;
  }
}

TEST(NetMaskToMaskLength, IPv4) {
  for (int i = 0; i <= 32; ++i) {
    SCOPED_TRACE(absl::Substitute("Failed on /$0", i));

    IPAddress mask;
    EXPECT_TRUE(MaskLengthToIPAddress(AF_INET, i, &mask));

    int length;
    EXPECT_TRUE(NetMaskToMaskLength(mask, &length));
    EXPECT_EQ(i, length);
  }
}

TEST(NetMaskToMaskLength, IPv6) {
  for (int i = 0; i <= 128; ++i) {
    SCOPED_TRACE(absl::Substitute("Failed on /$0", i));

    IPAddress mask;
    EXPECT_TRUE(MaskLengthToIPAddress(AF_INET6, i, &mask));

    int length;
    EXPECT_TRUE(NetMaskToMaskLength(mask, &length));
    EXPECT_EQ(i, length);
  }
}

}  //  namespace hercules
