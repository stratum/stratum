// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"

#include <dlfcn.h>

#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/strip.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

OnlpWrapper* OnlpWrapper::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex OnlpWrapper::init_lock_(absl::kConstInit);

constexpr int kOnlpBitmapBitsPerWord = 32;
constexpr int kOnlpBitmapWordCount = 8;

OidInfo::OidInfo(const onlp_oid_type_t type, OnlpPortNumber port,
                 HwState state) {
  oid_info_.id = ONLP_OID_TYPE_CREATE(type, port);
  oid_info_.status =
      (state == HW_STATE_PRESENT ? ONLP_OID_STATUS_FLAG_PRESENT
                                 : ONLP_OID_STATUS_FLAG_UNPLUGGED);
}

bool OidInfo::Present() const { return ONLP_OID_PRESENT(&oid_info_); }

HwState OidInfo::GetHardwareState() const {
  if (Present()) {
    if (ONLP_OID_STATUS_FLAG_IS_SET(&oid_info_, UNPLUGGED)) {
      return HW_STATE_OFF;  // FIXME(Yi): is this right?
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

OnlpWrapper* OnlpWrapper::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new OnlpWrapper();
    ::util::Status status = singleton_->Init();
    if (!status.ok()) {
      LOG(ERROR) << "OnlpWrapper::Init() failed: " << status;
      delete singleton_;
      singleton_ = nullptr;
    }
  }

  return singleton_;
}

OnlpWrapper::OnlpWrapper() {}

OnlpWrapper::~OnlpWrapper() {
  LOG(INFO) << "Deinitializing ONLP.";
  if (ONLP_FAILURE(onlp_functions_.onlp_sw_denit())) {
    LOG(ERROR) << "Failed to deinitialize ONLP.";
  }
  // TODO(max): log error
  dlclose(onlp_lib_handle_);
}

namespace {
template <typename T>
::util::StatusOr<T> load_symbol(void* handle, const char* name) {
  dlerror();  // Clear last error.
  auto* symbol = reinterpret_cast<T>(dlsym(handle, name));
  char* dl_err = dlerror();
  CHECK_RETURN_IF_FALSE(dl_err == nullptr)
      << "Failed to load symbol " << name << ": " << dl_err;

  return symbol;
}
}  // namespace

::util::Status OnlpWrapper::Init() {
  LOG(INFO) << "Initializing ONLP.";

  onlp_lib_handle_ = dlopen("libonlp.so", RTLD_NOW);
  CHECK_RETURN_IF_FALSE(onlp_lib_handle_ != nullptr)
      << "Failed to open libonlp.so: " << dlerror();

  // TODO(max): simplify with macro or something.
  ASSIGN_OR_RETURN(onlp_functions_.onlp_sw_init,
                   load_symbol<decltype(onlp_functions_.onlp_sw_init)>(
                       onlp_lib_handle_, "onlp_sw_init"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_sw_denit,
                   load_symbol<decltype(onlp_functions_.onlp_sw_denit)>(
                       onlp_lib_handle_, "onlp_sw_denit"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_oid_hdr_get_all,
                   load_symbol<decltype(onlp_functions_.onlp_oid_hdr_get_all)>(
                       onlp_lib_handle_, "onlp_oid_hdr_get_all"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_oid_get_all_free,
                   load_symbol<decltype(onlp_functions_.onlp_oid_get_all_free)>(
                       onlp_lib_handle_, "onlp_oid_get_all_free"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_oid_hdr_get,
                   load_symbol<decltype(onlp_functions_.onlp_oid_hdr_get)>(
                       onlp_lib_handle_, "onlp_oid_hdr_get"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_sfp_info_get,
                   load_symbol<decltype(onlp_functions_.onlp_sfp_info_get)>(
                       onlp_lib_handle_, "onlp_sfp_info_get"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_sfp_is_present,
                   load_symbol<decltype(onlp_functions_.onlp_sfp_is_present)>(
                       onlp_lib_handle_, "onlp_sfp_is_present"));
  ASSIGN_OR_RETURN(
      onlp_functions_.onlp_sfp_bitmap_t_init,
      load_symbol<decltype(onlp_functions_.onlp_sfp_bitmap_t_init)>(
          onlp_lib_handle_, "onlp_sfp_bitmap_t_init"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_sfp_bitmap_get,
                   load_symbol<decltype(onlp_functions_.onlp_sfp_bitmap_get)>(
                       onlp_lib_handle_, "onlp_sfp_bitmap_get"));
  ASSIGN_OR_RETURN(
      onlp_functions_.onlp_sfp_presence_bitmap_get,
      load_symbol<decltype(onlp_functions_.onlp_sfp_presence_bitmap_get)>(
          onlp_lib_handle_, "onlp_sfp_presence_bitmap_get"));

  ASSIGN_OR_RETURN(onlp_functions_.onlp_fan_info_get,
                   load_symbol<decltype(onlp_functions_.onlp_fan_info_get)>(
                       onlp_lib_handle_, "onlp_fan_info_get"));
  ASSIGN_OR_RETURN(
      onlp_functions_.onlp_fan_percentage_set,
      load_symbol<decltype(onlp_functions_.onlp_fan_percentage_set)>(
          onlp_lib_handle_, "onlp_fan_percentage_set"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_fan_rpm_set,
                   load_symbol<decltype(onlp_functions_.onlp_fan_rpm_set)>(
                       onlp_lib_handle_, "onlp_fan_rpm_set"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_fan_dir_set,
                   load_symbol<decltype(onlp_functions_.onlp_fan_dir_set)>(
                       onlp_lib_handle_, "onlp_fan_dir_set"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_thermal_info_get,
                   load_symbol<decltype(onlp_functions_.onlp_thermal_info_get)>(
                       onlp_lib_handle_, "onlp_thermal_info_get"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_led_info_get,
                   load_symbol<decltype(onlp_functions_.onlp_led_info_get)>(
                       onlp_lib_handle_, "onlp_led_info_get"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_led_mode_set,
                   load_symbol<decltype(onlp_functions_.onlp_led_mode_set)>(
                       onlp_lib_handle_, "onlp_led_mode_set"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_led_char_set,
                   load_symbol<decltype(onlp_functions_.onlp_led_char_set)>(
                       onlp_lib_handle_, "onlp_led_char_set"));
  ASSIGN_OR_RETURN(onlp_functions_.onlp_psu_info_get,
                   load_symbol<decltype(onlp_functions_.onlp_psu_info_get)>(
                       onlp_lib_handle_, "onlp_psu_info_get"));

  if (ONLP_FAILURE(onlp_functions_.onlp_sw_init(nullptr))) {
    LOG(FATAL) << "Failed to initialize ONLP.";
  }

  return ::util::OkStatus();
}

::util::StatusOr<OidInfo> OnlpWrapper::GetOidInfo(OnlpOid oid) const {
  onlp_oid_hdr_t oid_info = {};
  CHECK_RETURN_IF_FALSE(
      ONLP_SUCCESS(onlp_functions_.onlp_oid_hdr_get(oid, &oid_info)))
      << "Failed to get info for OID " << oid << ".";
  return OidInfo(oid_info);
}

::util::StatusOr<SfpInfo> OnlpWrapper::GetSfpInfo(OnlpOid oid) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_SFP(oid))
      << "Cannot get SFP info: OID " << oid << " is not an SFP.";
  // Default value of the SFP info
  onlp_sfp_info_t sfp_info = {{oid}};
  if (onlp_functions_.onlp_sfp_is_present(oid)) {
    CHECK_RETURN_IF_FALSE(
        ONLP_SUCCESS(onlp_functions_.onlp_sfp_info_get(oid, &sfp_info)))
        << "Failed to get SFP info for OID " << oid << ".";
  }
  return SfpInfo(sfp_info);
}

::util::StatusOr<FanInfo> OnlpWrapper::GetFanInfo(OnlpOid oid) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_FAN(oid))
      << "Cannot get FAN info: OID " << oid << " is not an FAN.";
  onlp_fan_info_t fan_info = {};
  CHECK_RETURN_IF_FALSE(
      ONLP_SUCCESS(onlp_functions_.onlp_fan_info_get(oid, &fan_info)))
      << "Failed to get FAN info for OID " << oid << ".";
  return FanInfo(fan_info);
}

::util::StatusOr<const onlp_fan_info_t*> FanInfo::GetOnlpFan() const {
  return &fan_info_;
}

::util::Status OnlpWrapper::SetFanPercent(OnlpOid oid, int value) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_FAN(oid))
      << "Cannot get FAN info: OID " << oid << " is not an FAN.";
  CHECK_RETURN_IF_FALSE(
      ONLP_SUCCESS(onlp_functions_.onlp_fan_percentage_set(oid, value)))
      << "Failed to set FAN percentage for OID " << oid << ".";
  return ::util::OkStatus();
}

::util::Status OnlpWrapper::SetFanRpm(OnlpOid oid, int val) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_FAN(oid))
      << "Cannot get FAN info: OID " << oid << " is not an FAN.";
  CHECK_RETURN_IF_FALSE(
      ONLP_SUCCESS(onlp_functions_.onlp_fan_rpm_set(oid, val)))
      << "Failed to set FAN rpm for OID " << oid << ".";
  return ::util::OkStatus();
}

::util::Status OnlpWrapper::SetFanDir(OnlpOid oid, FanDir dir) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_FAN(oid))
      << "Cannot set FAN info: OID " << oid << " is not an FAN.";
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(
      onlp_functions_.onlp_fan_dir_set(oid, static_cast<onlp_fan_dir_t>(dir))))
      << "Failed to set FAN direction for OID " << oid << ".";
  return ::util::OkStatus();
}

::util::StatusOr<ThermalInfo> OnlpWrapper::GetThermalInfo(OnlpOid oid) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_THERMAL(oid))
      << "Cannot get THERMAL info: OID " << oid << " is not an THERMAL.";
  onlp_thermal_info_t thermal_info = {};
  CHECK_RETURN_IF_FALSE(
      ONLP_SUCCESS(onlp_functions_.onlp_thermal_info_get(oid, &thermal_info)))
      << "Failed to get THERMAL info for OID " << oid << ".";
  return ThermalInfo(thermal_info);
}

::util::StatusOr<LedInfo> OnlpWrapper::GetLedInfo(OnlpOid oid) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_LED(oid))
      << "Cannot get LED info: OID " << oid << " is not an LED.";
  onlp_led_info_t led_info = {};
  CHECK_RETURN_IF_FALSE(
      ONLP_SUCCESS(onlp_functions_.onlp_led_info_get(oid, &led_info)))
      << "Failed to get LED info for OID " << oid << ".";
  return LedInfo(led_info);
}

::util::Status OnlpWrapper::SetLedMode(OnlpOid oid, LedMode mode) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_LED(oid))
      << "Cannot set LED info: OID " << oid << " is not an LED.";
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(onlp_functions_.onlp_led_mode_set(
      oid, static_cast<onlp_led_mode_t>(mode))))
      << "Failed to set LED mode for OID " << oid << ".";
  return ::util::OkStatus();
}

::util::Status OnlpWrapper::SetLedCharacter(OnlpOid oid, char val) const {
  CHECK_RETURN_IF_FALSE(ONLP_OID_IS_LED(oid))
      << "Cannot get LED info: OID " << oid << " is not an LED.";
  CHECK_RETURN_IF_FALSE(
      ONLP_SUCCESS(onlp_functions_.onlp_led_char_set(oid, val)))
      << "Failed to set LED character for OID " << oid << ".";
  return ::util::OkStatus();
}

::util::StatusOr<bool> OnlpWrapper::GetSfpPresent(OnlpOid port) const {
  return onlp_functions_.onlp_sfp_is_present(port);
}

::util::StatusOr<OnlpPresentBitmap> OnlpWrapper::GetSfpPresenceBitmap() const {
  OnlpPresentBitmap bitset;
  SfpBitmap presence;
  onlp_functions_.onlp_sfp_bitmap_t_init(&presence);
  CHECK_RETURN_IF_FALSE(
      ONLP_SUCCESS(onlp_functions_.onlp_sfp_presence_bitmap_get(&presence)))
      << "Failed to get presence bitmap ONLP.";
  int k = 0;
  for (int i = 0; i < kOnlpBitmapWordCount; i++) {
    for (int j = 0; j < kOnlpBitmapBitsPerWord; j++) {
      if (presence.hdr.words[i] & (1 << j))
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
  CHECK_RETURN_IF_FALSE(
      ONLP_SUCCESS(onlp_functions_.onlp_psu_info_get(oid, &psu_info)))
      << "Failed to get PSU info for OID " << oid << ".";
  return PsuInfo(psu_info);
}

::util::StatusOr<const onlp_psu_info_t*> PsuInfo::GetOnlpPsu() const {
  return &psu_info_;
}

::util::StatusOr<std::vector<OnlpOid>> OnlpWrapper::GetOidList(
    onlp_oid_type_flag_t type) const {
  std::vector<OnlpOid> oid_list;
  biglist_t* oid_hdr_list = nullptr;

  OnlpOid root_oid = ONLP_CHASSIS_ID_CREATE(1);
  CHECK_RETURN_IF_FALSE(ONLP_SUCCESS(
      onlp_functions_.onlp_oid_hdr_get_all(root_oid, type, 0, &oid_hdr_list)));

  // Iterate though the returned list and add the OIDs to oid_list
  biglist_t* curr_node = oid_hdr_list;
  while (curr_node != nullptr) {
    onlp_oid_hdr_t* oid_hdr =
        reinterpret_cast<onlp_oid_hdr_t*>(curr_node->data);
    oid_list.emplace_back(oid_hdr->id);
    curr_node = curr_node->next;
  }
  onlp_functions_.onlp_oid_get_all_free(oid_hdr_list);

  return oid_list;
}

::util::StatusOr<OnlpPortNumber> OnlpWrapper::GetSfpMaxPortNumber() const {
  SfpBitmap bitmap;
  onlp_functions_.onlp_sfp_bitmap_t_init(&bitmap);
  CHECK_RETURN_IF_FALSE(
      ONLP_SUCCESS(onlp_functions_.onlp_sfp_bitmap_get(&bitmap)))
      << "Failed to get valid SFP port bitmap from ONLP.";

  OnlpPortNumber port_num = ONLP_MAX_FRONT_PORT_NUM;
  int i, j;
  for (i = 0; i < kOnlpBitmapWordCount; ++i) {
    for (j = 0; j < kOnlpBitmapBitsPerWord; ++j) {
      if (bitmap.words[i] & (1 << j)) {
        port_num = i * kOnlpBitmapBitsPerWord + j + 1;
        // Note: return here only if the valid port numbers start from
        //       port 1 and are consecutive.
        // return port_num;
      }
    }
  }

  return port_num;
}

// Several converter functions.
// TODO(unknown): Revise the conversion logic here.
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
      // TODO(unknown): Need connector type (LC or MPO) which is missing.
    default:
      return MEDIA_TYPE_UNKNOWN;
  }
}

SfpType SfpInfo::GetSfpType() const {
  switch (sfp_info_.sff.sfp_type) {
    case SFF_SFP_TYPE_SFP28:
      return SFP_TYPE_SFP28;
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

SfpModuleType SfpInfo::GetSfpModuleType() const {
  switch (sfp_info_.sff.module_type) {
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

namespace {
absl::string_view TrimSuffix(absl::string_view str, absl::string_view suffix) {
  while (absl::ConsumeSuffix(&str, suffix)) {
  }
  return str;
}
}  // namespace

std::string SfpInfo::GetSfpVendor() const {
  return std::string(TrimSuffix(sfp_info_.sff.vendor, " "));
}

std::string SfpInfo::GetSfpModel() const {
  return std::string(TrimSuffix(sfp_info_.sff.model, " "));
}

std::string SfpInfo::GetSfpSerialNumber() const {
  return std::string(TrimSuffix(sfp_info_.sff.serial, " "));
}

void SfpInfo::GetModuleCaps(SfpModuleCaps* caps) const {
  // set all relevant capabilities flags
  caps->set_f_100(sfp_info_.sff.caps & SFF_MODULE_CAPS_F_100);
  caps->set_f_1g(sfp_info_.sff.caps & SFF_MODULE_CAPS_F_1G);
  caps->set_f_10g(sfp_info_.sff.caps & SFF_MODULE_CAPS_F_10G);
  caps->set_f_40g(sfp_info_.sff.caps & SFF_MODULE_CAPS_F_40G);
  caps->set_f_100g(sfp_info_.sff.caps & SFF_MODULE_CAPS_F_100G);
}

::util::StatusOr<const SffInfo*> SfpInfo::GetSffInfo() const {
  CHECK_RETURN_IF_FALSE(sfp_info_.sff.sfp_type != SFF_SFP_TYPE_INVALID)
      << "Cannot get SFF info: Invalid SFP type.";
  return &sfp_info_.sff;
}

FanDir FanInfo::GetFanDir() const {
  switch (fan_info_.dir) {
    case ONLP_FAN_DIR_B2F:
      return FAN_DIR_B2F;
    case ONLP_FAN_DIR_F2B:
      return FAN_DIR_F2B;
    default:
      return FAN_DIR_UNKNOWN;
  }
}

void FanInfo::GetCaps(FanCaps* caps) const {
  // set all relevant capabilities flags
  caps->set_set_dir(fan_info_.caps & ONLP_FAN_CAPS_SET_DIR);
  caps->set_get_dir(fan_info_.caps & ONLP_FAN_CAPS_GET_DIR);
  caps->set_set_rpm(fan_info_.caps & ONLP_FAN_CAPS_SET_RPM);
  caps->set_set_percentage(fan_info_.caps & ONLP_FAN_CAPS_SET_PERCENTAGE);
  caps->set_get_rpm(fan_info_.caps & ONLP_FAN_CAPS_GET_RPM);
  caps->set_get_percentage(fan_info_.caps & ONLP_FAN_CAPS_GET_PERCENTAGE);
}

PsuType PsuInfo::GetPsuType() const {
  switch (psu_info_.type) {
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

void PsuInfo::GetCaps(PsuCaps* caps) const {
  // set all relevant capabilities flags
  caps->set_get_type(psu_info_.caps & ONLP_PSU_CAPS_GET_TYPE);
  caps->set_get_vin(psu_info_.caps & ONLP_PSU_CAPS_GET_VIN);
  caps->set_get_vout(psu_info_.caps & ONLP_PSU_CAPS_GET_VOUT);
  caps->set_get_iin(psu_info_.caps & ONLP_PSU_CAPS_GET_IIN);
  caps->set_get_iout(psu_info_.caps & ONLP_PSU_CAPS_GET_IOUT);
  caps->set_get_pin(psu_info_.caps & ONLP_PSU_CAPS_GET_PIN);
  caps->set_get_pout(psu_info_.caps & ONLP_PSU_CAPS_GET_POUT);
}

int ThermalInfo::GetThermalCurTemp() const { return thermal_info_.mcelsius; }

int ThermalInfo::GetThermalWarnTemp() const {
  return thermal_info_.thresholds.warning;
}

int ThermalInfo::GetThermalErrorTemp() const {
  return thermal_info_.thresholds.error;
}

int ThermalInfo::GetThermalShutDownTemp() const {
  return thermal_info_.thresholds.shutdown;
}

void ThermalInfo::GetCaps(ThermalCaps* caps) const {
  // set all relevant capabilities flags
  caps->set_get_temperature(thermal_info_.caps &
                            ONLP_THERMAL_CAPS_GET_TEMPERATURE);
  caps->set_get_warning_threshold(thermal_info_.caps &
                                  ONLP_THERMAL_CAPS_GET_WARNING_THRESHOLD);
  caps->set_get_error_threshold(thermal_info_.caps &
                                ONLP_THERMAL_CAPS_GET_ERROR_THRESHOLD);
  caps->set_get_shutdown_threshold(thermal_info_.caps &
                                   ONLP_THERMAL_CAPS_GET_SHUTDOWN_THRESHOLD);
}

char LedInfo::GetLedChar() const { return led_info_.character; }

LedMode LedInfo::GetLedMode() const {
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

void LedInfo::GetCaps(LedCaps* caps) const {
  // set all relevant capabilities flags
  caps->set_off(led_info_.caps & ONLP_LED_CAPS_OFF);
  caps->set_auto_(led_info_.caps & ONLP_LED_CAPS_AUTO);
  caps->set_auto_blinking(led_info_.caps & ONLP_LED_CAPS_AUTO_BLINKING);
  caps->set_char_(led_info_.caps & ONLP_LED_CAPS_CHAR);
  caps->set_red(led_info_.caps & ONLP_LED_CAPS_RED);
  caps->set_red_blinking(led_info_.caps & ONLP_LED_CAPS_RED_BLINKING);
  caps->set_orange(led_info_.caps & ONLP_LED_CAPS_ORANGE);
  caps->set_orange_blinking(led_info_.caps & ONLP_LED_CAPS_ORANGE_BLINKING);
  caps->set_yellow(led_info_.caps & ONLP_LED_CAPS_YELLOW);
  caps->set_yellow_blinking(led_info_.caps & ONLP_LED_CAPS_YELLOW_BLINKING);
  caps->set_green(led_info_.caps & ONLP_LED_CAPS_GREEN);
  caps->set_green_blinking(led_info_.caps & ONLP_LED_CAPS_GREEN_BLINKING);
  caps->set_blue(led_info_.caps & ONLP_LED_CAPS_BLUE);
  caps->set_blue_blinking(led_info_.caps & ONLP_LED_CAPS_BLUE_BLINKING);
  caps->set_purple(led_info_.caps & ONLP_LED_CAPS_PURPLE);
  caps->set_purple_blinking(led_info_.caps & ONLP_LED_CAPS_PURPLE_BLINKING);
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
