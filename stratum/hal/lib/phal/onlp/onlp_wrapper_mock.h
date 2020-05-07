// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_MOCK_H_

#include <memory>
#include <vector>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class OnlpWrapperMock : public OnlpInterface {
 public:
  MOCK_CONST_METHOD1(GetOidInfo, ::util::StatusOr<OidInfo>(OnlpOid oid));
  MOCK_CONST_METHOD1(GetSfpInfo, ::util::StatusOr<SfpInfo>(OnlpOid oid));
  MOCK_CONST_METHOD1(GetFanInfo, ::util::StatusOr<FanInfo>(OnlpOid oid));
  MOCK_CONST_METHOD2(SetLedMode, ::util::Status(OnlpOid oid, LedMode mode));
  MOCK_CONST_METHOD2(SetLedCharacter, ::util::Status(OnlpOid oid, char val));
  MOCK_CONST_METHOD1(GetLedInfo, ::util::StatusOr<LedInfo>(OnlpOid oid));
  MOCK_CONST_METHOD2(SetFanPercent, ::util::Status(OnlpOid oid, int value));
  MOCK_CONST_METHOD2(SetFanRpm, ::util::Status(OnlpOid oid, int val));
  MOCK_CONST_METHOD2(SetFanDir, ::util::Status(OnlpOid oid, FanDir dir));
  MOCK_CONST_METHOD1(GetPsuInfo, ::util::StatusOr<PsuInfo>(OnlpOid oid));
  MOCK_CONST_METHOD1(GetThermalInfo,
                     ::util::StatusOr<ThermalInfo>(OnlpOid oid));
  MOCK_CONST_METHOD0(GetSfpPresenceBitmap,
                     ::util::StatusOr<OnlpPresentBitmap>());
  MOCK_CONST_METHOD1(GetSfpPresent, ::util::StatusOr<bool>(OnlpOid port));
  MOCK_CONST_METHOD0(GetSfpMaxPortNumber, ::util::StatusOr<OnlpPortNumber>());
  MOCK_CONST_METHOD1(GetOidList, ::util::StatusOr<std::vector<OnlpOid>>(
                                     onlp_oid_type_flag_t type));
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_MOCK_H_
