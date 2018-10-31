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

#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
extern "C" {
#include "sandblaze/onlp/include/onlp/oids.h"
#include "sandblaze/onlp/include/onlp/onlp.h"
#include "sandblaze/onlp/include/onlp/sfp.h"
}

#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/macros.h"
#include "absl/memory/memory.h"
#include "stratum/glue/status/status.h"
#include "util/task/statusor.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

::util::StatusOr<std::unique_ptr<OnlpWrapper>> OnlpWrapper::Make() {
  LOG(INFO) << "Initializing ONLP.";
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(onlp_sw_init(nullptr)))
      << "Failed to initialize ONLP.";
  return absl::WrapUnique(new OnlpWrapper());
}

OnlpWrapper::~OnlpWrapper() {
  LOG(INFO) << "Deinitializing ONLP.";
  if (ONLP_FAILURE(onlp_sw_denit())) {
    LOG(ERROR) << "Failed to deinitialize ONLP.";
  }
}

::util::StatusOr<OidInfo> OnlpWrapper::GetOidInfo(OnlpOid oid) const {
  onlp_oid_hdr_t oid_info = {};
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(onlp_oid_hdr_get(oid, &oid_info)))
      << "Failed to get info for OID " << oid << ".";
  return OidInfo(oid_info);
}

::util::StatusOr<SfpInfo> OnlpWrapper::GetSfpInfo(OnlpOid oid) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_SFP(oid))
      << "Cannot get SFP info: OID " << oid << " is not an SFP.";
  onlp_sfp_info_t sfp_info = {};
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(onlp_sfp_info_get(oid, &sfp_info)))
      << "Failed to get SFP info for OID " << oid << ".";
  return SfpInfo(sfp_info);
}

HwState OidInfo::GetHardwareState() const {
  switch (oid_info_.status) {
    case ONLP_OID_STATUS_FLAG_PRESENT:
      return HW_STATE_PRESENT;
    case ONLP_OID_STATUS_FLAG_FAILED:
      return HW_STATE_FAILED;
    case ONLP_OID_STATUS_FLAG_OPERATIONAL:
      return HW_STATE_READY;
    case ONLP_OID_STATUS_FLAG_UNPLUGGED:
      return HW_STATE_NOT_PRESENT;
  }
  return HW_STATE_UNKNOWN;
}

bool OidInfo::Present() const {
  return ONLP_OID_PRESENT(&oid_info_);
}

// Several converter functions.
// TODO: Revise the conversion logic here.
// Get MediaType from the given SFP connector type and SFF module type.
MediaType SfpInfo::GetMediaType() const {
  if (sfp_info_.type == ONLP_SFP_TYPE_SFP) {
    return MEDIA_TYPE_SFP;
  }
  // Others are of QSFP/QSFP++/QSFP28 type.
  switch (sfp_info_.sff.module_type) {
    case SFF_MODULE_TYPE_100G_BASE_SR4:
      return MEDIA_TYPE_QSFP_CSR4;
    case SFF_MODULE_TYPE_100G_BASE_LR4:
      return MEDIA_TYPE_QSFP_CLR4;
    case SFF_MODULE_TYPE_40G_BASE_CR4:
      return MEDIA_TYPE_QSFP_COPPER;
    case SFF_MODULE_TYPE_40G_BASE_SR4:
      return MEDIA_TYPE_QSFP_SR4;
    case SFF_MODULE_TYPE_40G_BASE_LR4:
      // TODO: Need connector type (LC or MPO) which is missing.
    default:
      return MEDIA_TYPE_UNKNOWN;
  }
}

::util::StatusOr<const SffInfo*> SfpInfo::GetSffInfo() const {
  CHECK_RETURN_IF_FALSE(sfp_info_.sff.sfp_type != SFF_SFP_TYPE_INVALID)
      << "Cannot get SFF info: Invalid SFP type.";
  return &sfp_info_.sff;
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
