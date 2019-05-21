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

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_H_

#include <bitset>
extern "C" {
#include <onlp/oids.h>
#include <onlp/sfp.h>
#include <onlp/fan.h>
#include <onlp/psu.h>
#include <onlp/thermal.h>
#include <onlp/led.h>
}
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/macros.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

#define ONLP_MAX_FRONT_PORT_NUM 256

typedef std::bitset<ONLP_MAX_FRONT_PORT_NUM> onlp_bitmap_t;
using OnlpPresentBitmap = onlp_bitmap_t;
using SfpBitmap = onlp_sfp_bitmap_t;
using OnlpOid = onlp_oid_t;
using OnlpOidHeader = onlp_oid_hdr_t;
using SffDomInfo = sff_dom_info_t;
using SffInfo = sff_info_t;
using OnlpSfpInfo = onlp_sfp_info_t;
using OnlpPortNumber = onlp_oid_t;

// This class encapsulates information that exists for every type of OID. More
// specialized classes for specific OID types should derive from this.
class OidInfo {
 public:
  explicit OidInfo(const onlp_oid_hdr_t& oid_info) : oid_info_(oid_info) {}
  explicit OidInfo(const onlp_oid_type_t type, OnlpPortNumber port,
                   HwState state) {
    oid_info_.id = ONLP_OID_TYPE_CREATE(type, port);
    oid_info_.status = (state == HW_STATE_PRESENT ?
        ONLP_OID_STATUS_FLAG_PRESENT : ONLP_OID_STATUS_FLAG_UNPLUGGED);
  }
  OidInfo() {}

  HwState GetHardwareState() const {
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

  bool Present() const {
    return ONLP_OID_PRESENT(&oid_info_);
  }

  const OnlpOidHeader* GetHeader() const { return &oid_info_; }
  uint32_t GetId() const { return (oid_info_.id & 0xFFFFFF); }

 private:
  onlp_oid_hdr_t oid_info_;
};

class SfpInfo : public OidInfo {
 public:
  explicit SfpInfo(const onlp_sfp_info_t& sfp_info)
      : OidInfo(sfp_info.hdr), sfp_info_(sfp_info) {}
  SfpInfo() {}

  MediaType GetMediaType() const {
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

  SfpType GetSfpType() const {
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

  SfpModuleType GetSfpModuleType() const {
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
  SfpModuleCaps GetSfpModuleCaps() const {
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

  // The lifetimes of pointers returned by these functions are managed by this
  // object. The returned pointer will never be nullptr.
  const SffDomInfo* GetSffDomInfo() const { return &sfp_info_.dom; }

  ::util::StatusOr<const SffInfo*> GetSffInfo() const {
    CHECK_RETURN_IF_FALSE(sfp_info_.sff.sfp_type != SFF_SFP_TYPE_INVALID)
          << "Cannot get SFF info: Invalid SFP type.";
    return &sfp_info_.sff;
  }

 private:
  onlp_sfp_info_t sfp_info_;
};

class FanInfo : public OidInfo {
 public:
  explicit FanInfo(const onlp_fan_info_t& fan_info)
      : OidInfo(fan_info.hdr), fan_info_(fan_info) {}
  FanInfo() {}
  FanDir GetFanDir() const {
    switch(fan_info_.dir) {
      case ONLP_FAN_DIR_B2F:
        return FAN_DIR_B2F;
      case ONLP_FAN_DIR_F2B:
        return FAN_DIR_F2B;
      default:
        return FAN_DIR_UNKNOWN;
    }
  }

  bool Capable(FanCaps fan_capability) const {
    int compare_caps;
    compare_caps = (fan_info_.caps & fan_capability);
    return compare_caps == fan_capability;
  }

  ::util::StatusOr<const onlp_fan_info_t*> GetOnlpFan() const {
    return &fan_info_;
  }

 private:
  onlp_fan_info_t fan_info_;
};

class PsuInfo : public OidInfo {
 public:
  explicit PsuInfo(const onlp_psu_info_t& psu_info)
      : OidInfo(psu_info.hdr), psu_info_(psu_info) {}
  PsuInfo() {}

  PsuType GetPsuType() const {
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

  bool Capable(PsuCaps psu_capability) const {
    int compare_caps;
    compare_caps = (psu_info_.caps & psu_capability);
    return compare_caps == psu_capability;
  }

  ::util::StatusOr<const onlp_psu_info_t*> GetOnlpPsu() const {
    return &psu_info_;
  }

 private:
  onlp_psu_info_t psu_info_;
};

class ThermalInfo : public OidInfo {
 public:
  explicit ThermalInfo(const onlp_thermal_info_t& thermal_info)
      : OidInfo(thermal_info.hdr), thermal_info_(thermal_info) {}
  ThermalInfo() {}
  int GetThermalCurTemp() const {
    return thermal_info_.mcelsius;
  }

  int GetThermalWarnTemp() const {
    return thermal_info_.thresholds.warning;
  }

  int GetThermalErrorTemp() const {
    return thermal_info_.thresholds.error;
  }

  int GetThermalShutDownTemp() const {
    return thermal_info_.thresholds.shutdown;
  }

  bool Capable(ThermalCaps thermal_capability) const {
    int compare_caps;
    compare_caps = (thermal_info_.caps & thermal_capability);
    return compare_caps == thermal_capability;
  }
  
 private:
  onlp_thermal_info_t thermal_info_;
};

class LedInfo : public OidInfo {
 public:
  explicit LedInfo(const onlp_led_info_t& led_info)
      : OidInfo(led_info.hdr), led_info_(led_info) {}
  LedInfo() {}
  LedMode GetLedMode() const {
    switch (led_info_.mode) {
      case ONLP_LED_MODE_OFF:
        return LED_MODE_OFF;
      case ONLP_LED_MODE_AUTO:
        return LED_MODE_AUTO;
      case ONLP_LED_MODE_AUTO_BLINKING:
        return LED_MODE_AUTO_BLINKING;
      case ONLP_LED_MODE_CHAR:
        return LED_MODE_CHAR;
      case ONLP_LED_MODE_RED:
        return LED_MODE_RED;
      case ONLP_LED_MODE_RED_BLINKING:
        return LED_MODE_RED_BLINKING;
      case ONLP_LED_MODE_ORANGE:
        return LED_MODE_ORANGE;
      case ONLP_LED_MODE_ORANGE_BLINKING:
        return LED_MODE_ORANGE_BLINKING;
      case ONLP_LED_MODE_YELLOW:
        return LED_MODE_YELLOW;
      case ONLP_LED_MODE_YELLOW_BLINKING:
        return LED_MODE_YELLOW_BLINKING;
      case ONLP_LED_MODE_GREEN:
        return LED_MODE_GREEN;
      case ONLP_LED_MODE_GREEN_BLINKING:
        return LED_MODE_GREEN_BLINKING;
      case ONLP_LED_MODE_BLUE:
        return LED_MODE_BLUE;
      case ONLP_LED_MODE_BLUE_BLINKING:
        return LED_MODE_BLUE_BLINKING;
      case ONLP_LED_MODE_PURPLE:
        return LED_MODE_PURPLE;
      case ONLP_LED_MODE_PURPLE_BLINKING:
        return LED_MODE_PURPLE_BLINKING;
      default:
        return LED_MODE_UNKNOWN;
    }
  }
  char GetLedChar() const {
    return led_info_.character;
  }
  bool Capable(LedCaps led_capability) const {
    return led_info_.caps & led_capability;
  }

 private:
  onlp_led_info_t led_info_;
};

// A interface for ONLP calls.
// This class wraps c-style direct ONLP calls with Google-style c++ calls that
// return ::util::Status.
class OnlpInterface {
 public:
  virtual ~OnlpInterface() {}

  // Given a OID object id, returns SFP info or failure.
  virtual ::util::StatusOr<SfpInfo> GetSfpInfo(OnlpOid oid) const = 0;

  // Given a OID object id, returns FAN info or failure.
  virtual ::util::StatusOr<FanInfo> GetFanInfo(OnlpOid oid) const = 0;

  // Given a OID object id, sets FAN percentage,
  // if FAN supports percentage capability.
  virtual ::util::Status
  SetFanPercent(OnlpOid oid, int value) const = 0;

  // Given a OID object id, sets FAN RPM,
  // if FAN supports RPM capability.
  virtual ::util::Status
  SetFanRpm(OnlpOid oid, int val) const = 0;

  // Given a OID object id, sets FAN Direction,
  // if FAN supports Direction capability.
  virtual ::util::Status
  SetFanDir(OnlpOid oid, FanDir dir) const = 0;

  // Given a OID object id, returns PSU info or failure.
  virtual ::util::StatusOr<PsuInfo> GetPsuInfo(OnlpOid oid) const = 0;

  // Given a OID object id, returns LED info or failure.
  virtual ::util::StatusOr<LedInfo> GetLedInfo(OnlpOid oid) const = 0;

  // Given a OID object id, returns THERMAL info or failure.
  virtual ::util::StatusOr<ThermalInfo> GetThermalInfo(OnlpOid oid) const = 0;
  // Given a OID object id, sets LED mode,
  // if LED supports color capability.
  virtual ::util::Status
  SetLedMode(OnlpOid oid, LedMode mode) const = 0;

  // Given a OID object id, sets LED character,
  // if LED supports character capability.
  virtual ::util::Status
  SetLedCharacter(OnlpOid oid, char val) const = 0;

  // Given an OID, returns the OidInfo for that object (or an error if it
  // doesn't exist
  virtual ::util::StatusOr<OidInfo> GetOidInfo(OnlpOid oid) const = 0;

  // Return list of onlp oids in the system based on the type.
  virtual ::util::StatusOr<std::vector <OnlpOid>> GetOidList(
      onlp_oid_type_flag_t type) const = 0;

  // Return whether a SFP with the given OID is present.
  virtual ::util::StatusOr<bool> GetSfpPresent(OnlpOid port) const = 0;

  // Return the presence bitmap for all SFP ports.
  virtual ::util::StatusOr<OnlpPresentBitmap> GetSfpPresenceBitmap() const = 0;

  // Get the maximum valid SFP port number.
  virtual ::util::StatusOr<OnlpPortNumber> GetSfpMaxPortNumber() const = 0;
};

// An OnlpInterface implementation that makes real calls into ONLP.
// Note that this wrapper performs ONLP setup and teardown, so only one may be
// allocated at any given time.
class OnlpWrapper : public OnlpInterface {
 public:
  static ::util::StatusOr<std::unique_ptr<OnlpWrapper>> Make();
  OnlpWrapper(const OnlpWrapper& other) = delete;
  OnlpWrapper& operator=(const OnlpWrapper& other) = delete;
  ~OnlpWrapper() override;

  ::util::StatusOr<OidInfo> GetOidInfo(OnlpOid oid) const override;
  ::util::StatusOr<PsuInfo> GetPsuInfo(OnlpOid oid) const override;
  ::util::StatusOr<SfpInfo> GetSfpInfo(OnlpOid oid) const override;
  ::util::StatusOr<FanInfo> GetFanInfo(OnlpOid oid) const override;
  ::util::Status SetFanPercent(OnlpOid oid, int value) const override;
  ::util::Status SetFanRpm(OnlpOid oid, int val) const override;
  ::util::Status SetFanDir(OnlpOid oid, FanDir dir) const override;
  ::util::StatusOr<ThermalInfo> GetThermalInfo(OnlpOid oid) const override;
  ::util::StatusOr<LedInfo> GetLedInfo(OnlpOid oid) const override;
  ::util::Status SetLedMode(OnlpOid oid, LedMode mode) const override;
  ::util::Status SetLedCharacter(OnlpOid oid, char val) const override;
  ::util::StatusOr<std::vector <OnlpOid>> GetOidList(
      onlp_oid_type_flag_t type) const override;
  ::util::StatusOr<bool> GetSfpPresent(OnlpOid port) const override;
  ::util::StatusOr<OnlpPresentBitmap> GetSfpPresenceBitmap() const override;
  ::util::StatusOr<OnlpPortNumber> GetSfpMaxPortNumber() const override;

 private:
  OnlpWrapper() {}
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_H_
