/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef STRATUM_HAL_LIB_COMMON_UTILS_H_
#define STRATUM_HAL_LIB_COMMON_UTILS_H_

#include <functional>
#include <string>

#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/public/proto/hal.pb.h"
#include "absl/base/integral_types.h"

namespace stratum {
namespace hal {

// Prints a Node message in a consistent and readable format. There are two
// versions for this function.
std::string PrintNode(const Node& n);
std::string PrintNode(uint64 id, int slot, int index);

// Prints a SingletonPort message in a consistent and readable format. There
// are two versions for this function.
std::string PrintSingletonPort(const SingletonPort& p);
std::string PrintSingletonPort(uint64 id, int slot, int port, int channel,
                               uint64 speed_bps);

// Prints PhysicalPort in readable format. e.g. "(slot: 1, port: 3)"
std::string PrintPhysicalPort(const PhysicalPort& physical_port);

// Prints PortState in a consistent format.
std::string PrintPortState(PortState state);

// Hash and comparator functions to use with container classes
// when port structures (PhysicalPort, SingletonPort, etc.) are used as
// the key or stored object type.

// A custom hash functor for SingletonPort
class SingletonPortHash {
 public:
  std::size_t operator()(const SingletonPort& port) const {
    size_t hash_val = 0;
    std::hash<int> integer_hasher;
    hash_val ^= integer_hasher(port.slot());
    hash_val ^= integer_hasher(port.port());
    hash_val ^= integer_hasher(port.channel());
    // Use middle 32 bits of speed_bps
    hash_val ^=
        integer_hasher(static_cast<int>((port.speed_bps() >> 16) & 0xFFFFFFFF));
    return hash_val;
  }
};

// A custom equal functor for SingletonPort
class SingletonPortEqual {
 public:
  bool operator()(const SingletonPort& lhs, const SingletonPort& rhs) const {
    return (lhs.slot() == rhs.slot()) && (lhs.port() == rhs.port()) &&
           (lhs.channel() == rhs.channel()) &&
           (lhs.speed_bps() == rhs.speed_bps());
  }
};

// Functor for comparing two SingletonPort instances based on slot, port,
// channel and speed_bps values in that order.
// Returns true if the first argument precedes the second in order,
// false otherwise.
class SingletonPortCompare {
 public:
  // Returns true if the first argument precedes the second; false otherwise.
  bool operator()(const SingletonPort& x, const SingletonPort& y) const {
    return ComparePorts(x, y);
  }

 private:
  // Compares slot, port, channel and speed_bps in that order.
  // Returns true if the first agrument precedes the second, false otherwise.
  bool ComparePorts(const SingletonPort& x, const SingletonPort& y) const {
    if (x.slot() != y.slot()) {
      return x.slot() < y.slot();
    } else if (x.port() != y.port()) {
      return x.port() < y.port();
    } else if (x.channel() != y.channel()) {
      return x.channel() < y.channel();
    } else {
      return x.speed_bps() < y.speed_bps();
    }
  }
};

// A custom hash functor for PhysicalPort
class PhysicalPortHash {
 public:
  std::size_t operator()(const PhysicalPort& physical_port) const {
    size_t h = 0;
    std::hash<int> integer_hasher;
    h ^= integer_hasher(physical_port.slot());
    h ^= integer_hasher(physical_port.port());
    return h;
  }
};

// A custom equal functor for PhysicalPort
class PhysicalPortEqual {
 public:
  bool operator()(const PhysicalPort& lhs, const PhysicalPort& rhs) const {
    return (lhs.slot() == rhs.slot()) && (lhs.port() == rhs.port());
  }
};

// Compares two PhysicalPort instances. Returns true if the first argument
// precedes the second in order, false otherwise.
class PhysicalPortCompare {
 public:
  // Returns true if the first argument precedes the second, false otherwise.
  bool operator()(const PhysicalPort& __x, const PhysicalPort& __y) const {
    return ComparePorts(__x, __y);
  }

 private:
  // Compares slot, port in order.
  // Returns true if the first agrument precedes the second, false otherwise.
  bool ComparePorts(const PhysicalPort& x, const PhysicalPort& y) const {
    if (x.slot() != y.slot()) {
      return x.slot() < y.slot();
    } else {
      return x.port() < y.port();
    }
  }
};

class PortUtils {
 public:
  // Builds a PhysicalPort object with the given field values.
  // No sanity checking is performed that the parameters are valid
  // for the switch.
  static PhysicalPort BuildPhysicalPort(int slot, int port);

  // Builds a SingletonPort object with the given field values.
  // No sanity checking is performed that the parameters are valid
  // for the switch.
  static SingletonPort BuildSingletonPort(int slot, int port, int channel,
                                          uint64 speed_bps);
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_UTILS_H_
