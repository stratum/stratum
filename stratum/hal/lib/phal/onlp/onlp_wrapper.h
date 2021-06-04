// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_H_

extern "C" {
#include <onlp/fan.h>
#include <onlp/led.h>
#include <onlp/oids.h>
#include <onlp/onlp.h>
#include <onlp/psu.h>
#include <onlp/sfp.h>
#include <onlp/thermal.h>
#include <onlp/i2c.h>
}

#include <bitset>
#include <memory>
#include <string>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/macros.h"

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
  OidInfo() = default;
  explicit OidInfo(const onlp_oid_hdr_t& oid_info) : oid_info_(oid_info) {}
  explicit OidInfo(const onlp_oid_type_t type, OnlpPortNumber port,
                   HwState state);

  HwState GetHardwareState() const;
  const OnlpOidHeader* GetHeader() const { return &oid_info_; }
  bool Present() const { return ONLP_OID_PRESENT(&oid_info_); }
  uint32_t GetId() const { return ONLP_OID_ID_GET(oid_info_.id); }
  uint8_t GetType() const { return ONLP_OID_TYPE_GET(oid_info_.id); }

 private:
  // Mutable because some ONLP macros do unfortunate non-const casts.
  mutable onlp_oid_hdr_t oid_info_;
};

class SfpInfo : public OidInfo {
 public:
  explicit SfpInfo(const onlp_sfp_info_t& sfp_info)
      : OidInfo(sfp_info.hdr), sfp_info_(sfp_info) {}
  SfpInfo() = default;
  MediaType GetMediaType() const;
  SfpType GetSfpType() const;
  SfpModuleType GetSfpModuleType() const;
  std::string GetSfpVendor() const;
  std::string GetSfpModel() const;
  std::string GetSfpSerialNumber() const;
  void GetModuleCaps(SfpModuleCaps* caps) const;

  // The lifetimes of pointers returned by these functions are managed by this
  // object. The returned pointer will never be nullptr.
  const SffDomInfo* GetSffDomInfo() const { return &sfp_info_.dom; }
  ::util::StatusOr<const SffInfo*> GetSffInfo() const;

 private:
  onlp_sfp_info_t sfp_info_;
};

class FanInfo : public OidInfo {
 public:
  explicit FanInfo(const onlp_fan_info_t& fan_info)
      : OidInfo(fan_info.hdr), fan_info_(fan_info) {}
  FanInfo() = default;
  FanDir GetFanDir() const;
  void GetCaps(FanCaps* caps) const;
  ::util::StatusOr<const onlp_fan_info_t*> GetOnlpFan() const;

 private:
  onlp_fan_info_t fan_info_;
};

class PsuInfo : public OidInfo {
 public:
  explicit PsuInfo(const onlp_psu_info_t& psu_info)
      : OidInfo(psu_info.hdr), psu_info_(psu_info) {}
  PsuInfo() = default;
  PsuType GetPsuType() const;
  void GetCaps(PsuCaps* caps) const;
  ::util::StatusOr<const onlp_psu_info_t*> GetOnlpPsu() const;

 private:
  onlp_psu_info_t psu_info_;
};

class ThermalInfo : public OidInfo {
 public:
  explicit ThermalInfo(const onlp_thermal_info_t& thermal_info)
      : OidInfo(thermal_info.hdr), thermal_info_(thermal_info) {}
  ThermalInfo() = default;
  int GetThermalCurTemp() const;
  int GetThermalWarnTemp() const;
  int GetThermalErrorTemp() const;
  int GetThermalShutDownTemp() const;
  void GetCaps(ThermalCaps* caps) const;

 private:
  onlp_thermal_info_t thermal_info_;
};

class LedInfo : public OidInfo {
 public:
  explicit LedInfo(const onlp_led_info_t& led_info)
      : OidInfo(led_info.hdr), led_info_(led_info) {}
  LedInfo() = default;
  LedMode GetLedMode() const;
  char GetLedChar() const;
  void GetCaps(LedCaps* caps) const;

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
  // Given a OID object id, sets SFP frequency.
  virtual ::util::Status SetSfpFrequency(OnlpOid oid, int port_number, int frequency) const = 0;

  // Given a OID object id, returns FAN info or failure.
  virtual ::util::StatusOr<FanInfo> GetFanInfo(OnlpOid oid) const = 0;

  // Given a OID object id, sets FAN percentage,
  // if FAN supports percentage capability.
  virtual ::util::Status SetFanPercent(OnlpOid oid, int value) const = 0;

  // Given a OID object id, sets FAN RPM,
  // if FAN supports RPM capability.
  virtual ::util::Status SetFanRpm(OnlpOid oid, int val) const = 0;

  // Given a OID object id, sets FAN Direction,
  // if FAN supports Direction capability.
  virtual ::util::Status SetFanDir(OnlpOid oid, FanDir dir) const = 0;

  // Given a OID object id, returns PSU info or failure.
  virtual ::util::StatusOr<PsuInfo> GetPsuInfo(OnlpOid oid) const = 0;

  // Given a OID object id, returns LED info or failure.
  virtual ::util::StatusOr<LedInfo> GetLedInfo(OnlpOid oid) const = 0;

  // Given a OID object id, returns THERMAL info or failure.
  virtual ::util::StatusOr<ThermalInfo> GetThermalInfo(OnlpOid oid) const = 0;
  // Given a OID object id, sets LED mode,
  // if LED supports color capability.
  virtual ::util::Status SetLedMode(OnlpOid oid, LedMode mode) const = 0;

  // Given a OID object id, sets LED character,
  // if LED supports character capability.
  virtual ::util::Status SetLedCharacter(OnlpOid oid, char val) const = 0;

  // Given an OID, returns the OidInfo for that object (or an error if it
  // doesn't exist
  virtual ::util::StatusOr<OidInfo> GetOidInfo(OnlpOid oid) const = 0;

  // Return list of onlp oids in the system based on the type.
  virtual ::util::StatusOr<std::vector<OnlpOid>> GetOidList(
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
  static OnlpWrapper* CreateSingleton();
  OnlpWrapper(const OnlpWrapper& other) = delete;
  OnlpWrapper& operator=(const OnlpWrapper& other) = delete;
  ~OnlpWrapper() override;

  // OnlpInterface public methods.
  ::util::StatusOr<OidInfo> GetOidInfo(OnlpOid oid) const override;
  ::util::StatusOr<PsuInfo> GetPsuInfo(OnlpOid oid) const override;
  ::util::StatusOr<SfpInfo> GetSfpInfo(OnlpOid oid) const override;
  ::util::Status SetSfpFrequency(OnlpOid oid, int port_number, int frequency) const override;
  ::util::StatusOr<FanInfo> GetFanInfo(OnlpOid oid) const override;
  ::util::Status SetFanPercent(OnlpOid oid, int value) const override;
  ::util::Status SetFanRpm(OnlpOid oid, int val) const override;
  ::util::Status SetFanDir(OnlpOid oid, FanDir dir) const override;
  ::util::StatusOr<ThermalInfo> GetThermalInfo(OnlpOid oid) const override;
  ::util::StatusOr<LedInfo> GetLedInfo(OnlpOid oid) const override;
  ::util::Status SetLedMode(OnlpOid oid, LedMode mode) const override;
  ::util::Status SetLedCharacter(OnlpOid oid, char val) const override;
  ::util::StatusOr<std::vector<OnlpOid>> GetOidList(
      onlp_oid_type_flag_t type) const override;
  ::util::StatusOr<bool> GetSfpPresent(OnlpOid port) const override;
  ::util::StatusOr<OnlpPresentBitmap> GetSfpPresenceBitmap() const override;
  ::util::StatusOr<OnlpPortNumber> GetSfpMaxPortNumber() const override;

 private:
  // Container to store the pointers to ONLP function into. Each member name
  // must match the ONLP symbol name.
  struct OnlpFunctions {
    int (*onlp_sw_init)(const char*);
    int (*onlp_sw_denit)(void);
    int (*onlp_oid_hdr_get_all)(onlp_oid_t root, onlp_oid_type_flags_t types,
                                uint32_t flags, biglist_t** list);
    int (*onlp_oid_get_all_free)(biglist_t* list);
    int (*onlp_oid_hdr_get)(onlp_oid_t oid, onlp_oid_hdr_t* hdr);
    int (*onlp_sfp_info_get)(onlp_oid_t port, onlp_sfp_info_t* info);
    int (*onlp_sfp_is_present)(onlp_oid_t port);
    void (*onlp_sfp_bitmap_t_init)(onlp_sfp_bitmap_t* bmap);
    int (*onlp_sfp_bitmap_get)(onlp_sfp_bitmap_t* bmap);
    int (*onlp_sfp_presence_bitmap_get)(onlp_sfp_bitmap_t* dst);
    int (*onlp_fan_info_get)(onlp_oid_t oid, onlp_fan_info_t* rv);
    int (*onlp_fan_percentage_set)(onlp_oid_t oid, int p);
    int (*onlp_fan_rpm_set)(onlp_oid_t oid, int rpm);
    int (*onlp_fan_dir_set)(onlp_oid_t oid, onlp_fan_dir_t dir);
    int (*onlp_thermal_info_get)(onlp_oid_t oid, onlp_thermal_info_t* rv);
    int (*onlp_led_info_get)(onlp_oid_t oid, onlp_led_info_t* rv);
    int (*onlp_led_mode_set)(onlp_oid_t oid, onlp_led_mode_t mode);
    int (*onlp_led_char_set)(onlp_oid_t oid, char c);
    int (*onlp_psu_info_get)(onlp_oid_t oid, onlp_psu_info_t* rv);
    int (*onlp_i2c_mux_mapping)(int port_number, int reset);
    int (*onlp_i2c_write)(int bus, uint8_t addr, uint8_t offset, int size, uint8_t* data, uint32_t flags);
    int (*onlp_i2c_readb)(int bus, uint8_t addr, uint8_t offset, uint32_t flags);
    OnlpFunctions()
        : onlp_sw_init(nullptr),
          onlp_sw_denit(nullptr),
          onlp_oid_hdr_get_all(nullptr),
          onlp_oid_get_all_free(nullptr),
          onlp_oid_hdr_get(nullptr),
          onlp_sfp_info_get(nullptr),
          onlp_sfp_is_present(nullptr),
          onlp_sfp_bitmap_t_init(nullptr),
          onlp_sfp_bitmap_get(nullptr),
          onlp_sfp_presence_bitmap_get(nullptr),
          onlp_fan_info_get(nullptr),
          onlp_fan_percentage_set(nullptr),
          onlp_fan_rpm_set(nullptr),
          onlp_fan_dir_set(nullptr),
          onlp_thermal_info_get(nullptr),
          onlp_led_info_get(nullptr),
          onlp_led_mode_set(nullptr),
          onlp_led_char_set(nullptr),
          onlp_psu_info_get(nullptr),
          oonlp_i2c_mux_mapping(nullptr),
          onlp_i2c_write(nullptr),
          onlp_i2c_readb(nullptr) {}
  };
  // Private constructor. Use CreateInstance instead.
  OnlpWrapper();

  // Initializes the internals of the OnlpWrapper. Called by CreateSingleton.
  ::util::Status Initialize();

  // The lock used for initialization of the singleton.
  static absl::Mutex init_lock_;

  // Handle to the ONLP library.
  void* onlp_lib_handle_;

  // Container to store the resolved symbols to ONLP functions. Initialized by
  // Initialize().
  OnlpFunctions onlp_functions_;

  // The singleton instance.
  static OnlpWrapper* singleton_ GUARDED_BY(init_lock_);
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_H_
