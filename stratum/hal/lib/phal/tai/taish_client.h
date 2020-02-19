// Copyright 2020-present Open Networking Foundation
// Copyright 2020 PLVision
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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAISH_CLIENT_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAISH_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "grpc/grpc.h"
#include "grpcpp/channel.h"
#include "grpcpp/client_context.h"
#include "grpcpp/create_channel.h"
#include "grpcpp/security/credentials.h"

#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/macros.h"

#include "stratum/hal/lib/phal/tai/taish.grpc.pb.h"
#include "stratum/hal/lib/phal/tai/taish.pb.h"

namespace stratum {
namespace hal {
namespace phal {

/*!
 * \brief Hostif class represents the TAI Host interface
 */
class Hostif {
 public:
  Hostif() = default;
  Hostif(uint64 object_id, uint32 index)
      : object_id_(object_id), index_(index) {}

  uint64 object_id_;
  uint32 index_;

  taish::TAIObjectType object_type{taish::TAIObjectType::HOSTIF};
};

/*!
 * \brief Netif class represents the TAI Network interface
 */
class Netif {
 public:
  Netif() = default;
  Netif(uint64 object_id, uint32 index)
      : object_id_(object_id), index_(index) {}

  uint64 object_id_;
  uint32 index_;

  taish::TAIObjectType object_type{taish::TAIObjectType::NETIF};
};

/*!
 * \brief Module class represents the TAI module
 */
class Module {
 public:
  Module() = default;
  Module(uint64 object_id, const std::string& location)
      : object_id_(object_id), location_(location) {}

  void AddNetif(uint64 object_id, uint32 index) {
    netifs.push_back({object_id, index});
  }

  void AddHostif(uint64 object_id, uint32 index) {
    hostifs.push_back({object_id, index});
  }

  uint64 object_id_;
  std::string location_;

  std::vector<Netif> netifs;
  std::vector<Hostif> hostifs;

  taish::TAIObjectType object_type{taish::TAIObjectType::MODULE};
};

/*!
 * \brief TaishClient class makes get/set value to TAI library using gRPC calls
 * to TAI server
 */
class TaishClient {
 public:
  explicit TaishClient(std::shared_ptr<grpc::Channel> channel);

  util::StatusOr<std::string> GetValue(const std::string& location,
                                       int network_index,
                                       const std::string& attr_name);
  util::Status SetValue(const std::string& location, int network_index,
                        const std::string& attr_name, const std::string& value);

 private:
  void GetListModule();
  ::taish::AttributeMetadata GetMetadata(taish::TAIObjectType object_type,
                                         const std::string& attr_name);
  std::vector<::taish::AttributeMetadata> ListMetadata(
      taish::TAIObjectType object_type);

 private:
  std::unique_ptr<taish::TAI::Stub> taish_;

  std::vector<Module> modules_;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAISH_CLIENT_H_
