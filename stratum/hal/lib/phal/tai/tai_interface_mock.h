// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_INTERFACE_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_INTERFACE_MOCK_H_

#include <vector>

#include "gmock/gmock.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/tai/tai_interface.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

class TaiInterfaceMock : public TaiInterface {
 public:
  MOCK_METHOD0(Initialize, ::util::Status());
  MOCK_METHOD0(GetModuleIds, util::StatusOr<std::vector<uint64>>());
  MOCK_METHOD1(GetNetworkInterfaceIds,
               util::StatusOr<std::vector<uint64>>(const uint64 module_id));
  MOCK_METHOD1(GetHostInterfaceIds,
               util::StatusOr<std::vector<uint64>>(const uint64 module_id));
  MOCK_METHOD1(GetTxLaserFrequency,
               util::StatusOr<uint64>(const uint64 netif_id));
  MOCK_METHOD1(GetCurrentInputPower,
               util::StatusOr<double>(const uint64 netif_id));
  MOCK_METHOD1(GetCurrentOutputPower,
               util::StatusOr<double>(const uint64 netif_id));
  MOCK_METHOD1(GetTargetOutputPower,
               util::StatusOr<double>(const uint64 netif_id));
  MOCK_METHOD1(GetModulationFormat,
               util::StatusOr<uint64>(const uint64 netif_id));
  MOCK_METHOD2(SetTargetOutputPower,
               util::Status(const uint64 netif_id, const double power));
  MOCK_METHOD2(SetModulationFormat,
               util::Status(const uint64 netif_id, const uint64 mod_format));
  MOCK_METHOD2(SetTxLaserFrequency,
               util::Status(const uint64 netif_id, const uint64 frequency));
  MOCK_METHOD0(Shutdown, util::Status());
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_INTERFACE_MOCK_H_
