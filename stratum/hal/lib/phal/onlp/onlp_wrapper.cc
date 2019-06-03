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

OidInfo::OidInfo(const onlp_oid_type_t type, OnlpPortNumber port,
                 HwState state) {
  oid_info_.id = ONLP_OID_TYPE_CREATE(type, port);
  oid_info_.status = (state == HW_STATE_PRESENT ?
      ONLP_OID_STATUS_FLAG_PRESENT : ONLP_OID_STATUS_FLAG_UNPLUGGED);
};

bool OidInfo::Present() const {
  return ONLP_OID_PRESENT(&oid_info_);
}

HwState OidInfo::GetHardwareState() const {
  if (Present()) {
    if (ONLP_OID_STATUS_FLAG_IS_SET(&oid_info_, UNPLUGGED)) {
      return HW_STATE_OFF; // FIXME(Yi): is this right?
    }
    if (ONLP_OID_STATUS_FLAG_IS_SET(&oid_info_, FAILED)) {
      return HW_STATE_FAILED;
    }
    if (ONLP_OID_STATUS_FLAG_IS_SET(&oid_info_, OPERATIONAL)) {
      return HW_STATE_READY;
    }
    return HW_STATE_PRESENT;
  }

  return HW_STATE_NOT_PRESENT;
}

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
  // Default value of the SFP info
  onlp_sfp_info_t sfp_info = {{oid}};
  if (onlp_sfp_is_present(oid)) {
    CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(onlp_sfp_info_get(oid, &sfp_info)))
          << "Failed to get SFP info for OID " << oid << ".";
  }
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

::util::Status OnlpWrapper::
  SetFanPercent(OnlpOid oid, int value) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_FAN(oid))
      << "Cannot get FAN info: OID " << oid << " is not an FAN.";
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(onlp_fan_percentage_set(oid, value)))
      << "Failed to set FAN percentage for OID " << oid << ".";
  return ::util::OkStatus();
}

::util::Status OnlpWrapper::
  SetFanRpm(OnlpOid oid, int val) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_FAN(oid))
      << "Cannot get FAN info: OID " << oid << " is not an FAN.";
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(onlp_fan_rpm_set(oid, val)))
      << "Failed to set FAN rpm for OID " << oid << ".";
  return ::util::OkStatus();
}

::util::Status OnlpWrapper::
  SetFanDir(OnlpOid oid, FanDir dir) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_FAN(oid))
      << "Cannot set FAN info: OID " << oid << " is not an FAN.";
  CHECK_RETURN_IF_FALSE(
      ONLP_SUCCESS(onlp_fan_dir_set(oid, static_cast<onlp_fan_dir_t>(dir))))
      << "Failed to set FAN direction for OID " << oid << ".";
  return ::util::OkStatus();
}

::util::StatusOr<ThermalInfo> OnlpWrapper::GetThermalInfo(OnlpOid oid) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_THERMAL(oid))
        << "Cannot get THERMAL info: OID " << oid << " is not an THERMAL.";
  onlp_thermal_info_t thermal_info = {};
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(onlp_thermal_info_get(oid, &thermal_info)))
        << "Failed to get THERMAL info for OID " << oid << ".";
  return ThermalInfo(thermal_info);
}

::util::StatusOr<LedInfo> OnlpWrapper::GetLedInfo(OnlpOid oid) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_LED(oid))
      << "Cannot get LED info: OID " << oid << " is not an LED.";
  onlp_led_info_t led_info = {};
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(onlp_led_info_get(oid, &led_info)))
      << "Failed to get LED info for OID " << oid << ".";
  return LedInfo(led_info);
}

::util::Status OnlpWrapper::
  SetLedMode(OnlpOid oid, LedMode mode) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_LED(oid))
      << "Cannot set LED info: OID " << oid << " is not an LED.";
  CHECK_RETURN_IF_FALSE(
      ONLP_SUCCESS(onlp_led_mode_set(oid, static_cast<onlp_led_mode_t>(mode))))
      << "Failed to set LED mode for OID " << oid << ".";
  return ::util::OkStatus();
}

::util::Status OnlpWrapper::
  SetLedCharacter(OnlpOid oid, char val) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_LED(oid))
      << "Cannot get LED info: OID " << oid << " is not an LED.";
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(onlp_led_char_set(oid, val)))
      << "Failed to set LED character for OID " << oid << ".";
  return ::util::OkStatus();
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

::util::StatusOr<std::vector<OnlpOid>> OnlpWrapper::GetOidList(
      onlp_oid_type_flag_t type) const {

  std::vector<OnlpOid> oid_list;
  biglist_t* oid_hdr_list;

  OnlpOid root_oid = ONLP_CHASSIS_ID_CREATE(1);
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

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
