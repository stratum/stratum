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
#include "onlp/onlp.h"
}

#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/macros.h"
#include "absl/memory/memory.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

constexpr int kOnlpBitmapBitsPerWord = 32;
constexpr int kOnlpBitmapWordCount = 8;

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

::util::StatusOr<FanInfo> OnlpWrapper::GetFanInfo(OnlpOid oid) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_FAN(oid))
      << "Cannot get FAN info: OID " << oid << " is not an FAN.";
  onlp_fan_info_t fan_info = {};
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(onlp_fan_info_get(oid, &fan_info)))
      << "Failed to get FAN info for OID " << oid << ".";
  return FanInfo(fan_info);
}

::util::StatusOr<const onlp_fan_info_t*> FanInfo::GetOnlpFan() const {
  return &fan_info_;
}

::util::StatusOr<bool> OnlpWrapper::GetSfpPresent(OnlpOid port) const {
  return onlp_sfp_is_present(port);
}

::util::StatusOr<OnlpPresentBitmap> OnlpWrapper::GetSfpPresenceBitmap() const {
  OnlpPresentBitmap bitset;
  SfpBitmap presence;
  onlp_sfp_bitmap_t_init(&presence);
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(onlp_sfp_presence_bitmap_get(&presence)))
           << "Failed to get presence bitmap ONLP.";
  int k=0;
  for(int i=0; i<kOnlpBitmapWordCount; i++){
    for(int j=0; j<kOnlpBitmapBitsPerWord; j++) {
      if( presence.hdr.words[i]&(1<<j))
        bitset.set(k);
      else
        bitset.reset(k);

      k++;
    }
  }
  return bitset; 
}

::util::StatusOr<PsuInfo> OnlpWrapper::GetPsuInfo(OnlpOid oid) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_PSU(oid))
      << "Cannot get PSU info: OID " << oid << " is not an PSU.";
  onlp_psu_info_t psu_info = {};
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(onlp_psu_info_get(oid, &psu_info)))
      << "Failed to get PSU info for OID " << oid << ".";
  return PsuInfo(psu_info);
}

::util::StatusOr<const onlp_psu_info_t*> PsuInfo::GetOnlpPsu() const {
  return &psu_info_;
}

::util::StatusOr<std::vector<OnlpOid>> OnlpWrapper::GetOidList(
      onlp_oid_type_flag_t type) const {

  std::vector<OnlpOid> oid_list;
  biglist_t* oid_hdr_list;

  OnlpOid root_oid = ONLP_SFP_ID_CREATE(1);
  onlp_oid_hdr_get_all(root_oid, type, 0, &oid_hdr_list);

  // Iterate though the returned list and add the OIDs to oid_list
  biglist_t* curr_node = oid_hdr_list;
  while (curr_node != nullptr) {
    onlp_oid_hdr_t* oid_hdr = (onlp_oid_hdr_t*) curr_node->data;
    oid_list.emplace_back(oid_hdr->id);
    curr_node = curr_node->next;
  }
  onlp_oid_get_all_free(oid_hdr_list);

  return oid_list;
}

::util::StatusOr<OnlpPortNumber> OnlpWrapper::GetSfpMaxPortNumber() const {
  SfpBitmap bitmap;
  onlp_sfp_bitmap_t_init(&bitmap);
  int result = onlp_sfp_bitmap_get(&bitmap);
  if(result < 0) {
    LOG(ERROR) << "Failed to get valid SFP port bitmap from ONLP.";
  }

  OnlpPortNumber port_num = ONLP_MAX_FRONT_PORT_NUM;
  int i, j;
  for (i = 0; i < kOnlpBitmapWordCount; i ++) {
    for (j = 0; j < kOnlpBitmapBitsPerWord; j ++) {
      if (bitmap.words[i] & (1<<j)) {
        port_num = i * kOnlpBitmapBitsPerWord + j + 1;
        // Note: return here only if the valid port numbers start from
        //       port 1 and are consecutive.
        //return port_num;
      }
    }
  }

  return port_num;
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

SfpType SfpInfo::GetSfpType() const {
  switch(sfp_info_.sff.sfp_type) {
  case SFF_SFP_TYPE_SFP:
    return SFP_TYPE_SFP;
  case SFF_SFP_TYPE_QSFP:
    return SFP_TYPE_QSFP;
  case SFF_SFP_TYPE_QSFP_PLUS:
    return SFP_TYPE_QSFP_PLUS;
  case SFF_SFP_TYPE_QSFP28:
    return SFP_TYPE_QSFP28;
  default:
    return SFP_TYPE_UNKNOWN;
  }
}

SfpModuleType SfpInfo::GetSfpModuleType()const {
  switch(sfp_info_.sff.module_type) {
  case SFF_MODULE_TYPE_100G_BASE_CR4:
    return SFP_MODULE_TYPE_100G_BASE_CR4;
  case SFF_MODULE_TYPE_10G_BASE_CR:
    return SFP_MODULE_TYPE_10G_BASE_CR;
  case SFF_MODULE_TYPE_1G_BASE_SX:
    return SFP_MODULE_TYPE_1G_BASE_SX;
  default:
    return SFP_MODULE_TYPE_UNKNOWN;
  }
}

SfpModuleCaps SfpInfo::GetSfpModuleCaps() const {
  switch(sfp_info_.sff.caps) {
  case SFF_MODULE_CAPS_F_100:
    return SFP_MODULE_CAPS_F_100;
  case SFF_MODULE_CAPS_F_1G:
    return SFP_MODULE_CAPS_F_1G;
  case SFF_MODULE_CAPS_F_10G:
    return SFP_MODULE_CAPS_F_10G;
  case SFF_MODULE_CAPS_F_40G:
    return SFP_MODULE_CAPS_F_40G;
  case SFF_MODULE_CAPS_F_100G:
    return SFP_MODULE_CAPS_F_100G;
  default:
    return SFP_MODULE_CAPS_UNKNOWN;
  }
}

::util::StatusOr<const SffInfo*> SfpInfo::GetSffInfo() const {
  CHECK_RETURN_IF_FALSE(sfp_info_.sff.sfp_type != SFF_SFP_TYPE_INVALID)
      << "Cannot get SFF info: Invalid SFP type.";
  return &sfp_info_.sff;
}

FanDir FanInfo::GetFanDir() const {
  switch(fan_info_.dir) {
  case ONLP_FAN_DIR_B2F:
    return FAN_DIR_B2F;
  case ONLP_FAN_DIR_F2B:
    return FAN_DIR_F2B;
  default:
    return FAN_DIR_UNKNOWN;
  }
}

FanCaps FanInfo::GetFanCaps() const {
  switch(fan_info_.caps) {
  case ONLP_FAN_CAPS_SET_DIR:
    return FAN_CAPS_SET_DIR;
  case ONLP_FAN_CAPS_GET_DIR:
    return FAN_CAPS_GET_DIR;
  case ONLP_FAN_CAPS_SET_RPM:
    return FAN_CAPS_SET_RPM;
  case  ONLP_FAN_CAPS_SET_PERCENTAGE:
    return FAN_CAPS_SET_PERCENTAGE;
  case ONLP_FAN_CAPS_GET_RPM:
    return FAN_CAPS_GET_RPM;
  case ONLP_FAN_CAPS_GET_PERCENTAGE:
    return FAN_CAPS_GET_PERCENTAGE;
  default:
    return FAN_CAPS_UNKNOWN;
  }
}

PsuType PsuInfo::GetPsuType() const {
  switch(psu_info_.type) {
  case ONLP_PSU_TYPE_AC:
    return PSU_TYPE_AC;
  case ONLP_PSU_TYPE_DC12:
    return PSU_TYPE_DC12;
  case ONLP_PSU_TYPE_DC48:
    return PSU_TYPE_DC48;
  default:
    return PSU_TYPE_UNKNOWN;
  }
}

PsuCaps PsuInfo::GetPsuCaps() const {
  switch(psu_info_.caps) {
  case ONLP_PSU_CAPS_GET_TYPE:
    return PSU_CAPS_GET_TYPE;
  case ONLP_PSU_CAPS_GET_VIN:
    return PSU_CAPS_GET_VIN;
  case ONLP_PSU_CAPS_GET_VOUT:
    return PSU_CAPS_GET_VOUT;
  case  ONLP_PSU_CAPS_GET_IIN:
    return PSU_CAPS_GET_IIN;
  case ONLP_PSU_CAPS_GET_IOUT:
    return PSU_CAPS_GET_IOUT;
  case ONLP_PSU_CAPS_GET_PIN:
    return PSU_CAPS_GET_PIN;
  case ONLP_PSU_CAPS_GET_POUT:
    return PSU_CAPS_GET_POUT;
  default:
    return PSU_CAPS_UNKNOWN;
  }
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
