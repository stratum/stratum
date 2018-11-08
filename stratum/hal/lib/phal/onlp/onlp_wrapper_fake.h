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

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_FAKE_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_FAKE_H_

// Fake definitions of the OnlpInterface and the OnlpWrapper class.
// It hides the dependency on ONLP library for unit testing of the
// OnlpEventHandler and Onlphal.

//extern "C" {
//#include "sandblaze/onlp/include/onlp/oids.h"
//#include "sandblaze/onlp/include/onlp/sfp.h"
//}
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/macros.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "absl/memory/memory.h"
#include <bitset>

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

// This section provides definitions of ONLP types and data structures that
// are required in OnlpWrapper - to avoid ONLP library dependency for
// unit testing.

// From onlp/oids.h
typedef uint32_t onlp_oid_t;
//typedef uint32_t onlp_oid_id_t;
typedef uint32_t onlp_oid_status_flags_t;

#define ONLP_OID_DESC_SIZE 128
typedef char onlp_oid_desc_t[ONLP_OID_DESC_SIZE];

#define ONLP_OID_TABLE_SIZE 32
typedef onlp_oid_t onlp_oid_table_t[ONLP_OID_TABLE_SIZE];

typedef enum onlp_oid_type_e {
    ONLP_OID_TYPE_CHASSIS = 1,
    ONLP_OID_TYPE_MODULE = 2,
    ONLP_OID_TYPE_THERMAL = 3,
    ONLP_OID_TYPE_FAN = 4,
    ONLP_OID_TYPE_PSU = 5,
    ONLP_OID_TYPE_LED = 6,
    ONLP_OID_TYPE_SFP = 7,
    ONLP_OID_TYPE_GENERIC = 8,
} onlp_oid_type_t;
#define ONLP_OID_TYPE_CREATE(_type, _id) ( ( (_type) << 24) | (_id))

typedef enum onlp_oid_type_flag_e {
    ONLP_OID_TYPE_FLAG_CHASSIS = (1 << 1),
    ONLP_OID_TYPE_FLAG_MODULE = (1 << 2),
    ONLP_OID_TYPE_FLAG_THERMAL = (1 << 3),
    ONLP_OID_TYPE_FLAG_FAN = (1 << 4),
    ONLP_OID_TYPE_FLAG_PSU = (1 << 5),
    ONLP_OID_TYPE_FLAG_LED = (1 << 6),
    ONLP_OID_TYPE_FLAG_SFP = (1 << 7),
    ONLP_OID_TYPE_FLAG_GENERIC = (1 << 8),
} onlp_oid_type_flag_t;
typedef uint32_t onlp_oid_type_flags_t;

typedef enum onlp_oid_status_flag_e {
    ONLP_OID_STATUS_FLAG_PRESENT = (1 << 0),
    ONLP_OID_STATUS_FLAG_FAILED = (1 << 1),
    ONLP_OID_STATUS_FLAG_OPERATIONAL = (1 << 2),
    ONLP_OID_STATUS_FLAG_UNPLUGGED = (1 << 3),
} onlp_oid_status_flag_t;

typedef struct onlp_oid_hdr_s {
    /** The OID */
    onlp_oid_t id;
    /** The description of this object. */
    onlp_oid_desc_t description;
    /** The parent OID of this object. */
    onlp_oid_t poid;
    /** The children of this OID */
    onlp_oid_table_t coids;
    /** The current status (if applicable) */
    onlp_oid_status_flags_t status;
} onlp_oid_hdr_t;
//

// From sff/dom.h
#define SFF_DOM_CHANNEL_COUNT_MAX 4

typedef enum sff_dom_spec_e {
    SFF_DOM_SPEC_UNSUPPORTED,
    SFF_DOM_SPEC_SFF8436,
    SFF_DOM_SPEC_SFF8472,
    SFF_DOM_SPEC_SFF8636,
    SFF_DOM_SPEC_LAST = SFF_DOM_SPEC_SFF8636,
    SFF_DOM_SPEC_COUNT,
    SFF_DOM_SPEC_INVALID = -1,
} sff_dom_spec_t;

typedef struct sff_dom_channel_info_s {
    /** Valid Field Flags - a bitfield of sff_dom_field_flag_t */
    uint32_t fields;
    /** Measured bias current in 2uA units */
    uint16_t bias_cur;
    /** Measured Rx Power (Avg Optical Power) */
    uint16_t rx_power;
    /** Measured RX Power (OMA) */
    uint16_t rx_power_oma;
    /** Measured TX Power (Avg Optical Power) */
    uint16_t tx_power;
} sff_dom_channel_info_t;

typedef struct sff_dom_info_s {
    /** The SFF Specification from which this information was derived. */
    sff_dom_spec_t spec;
    /** Valid Field Flags - a bitfield of sff_domf_field_flag_t */
    uint32_t fields;
    /** Temp in 16-bit signed 1/256 Celsius */
    int16_t temp;
    /** Voltage in 0.1mV units */
    uint16_t voltage;
    /** Whether external calibration was enabled. */
    int extcal;
    /** Number of reporting channels. */
    int nchannels;
    /** Channel information. */
    sff_dom_channel_info_t channels[SFF_DOM_CHANNEL_COUNT_MAX];
} sff_dom_info_t;
//

// From sff/sff.h
typedef enum sff_sfp_type_e {
    SFF_SFP_TYPE_SFP,
    SFF_SFP_TYPE_QSFP,
    SFF_SFP_TYPE_QSFP_PLUS,
    SFF_SFP_TYPE_QSFP28,
    SFF_SFP_TYPE_SFP28,
    SFF_SFP_TYPE_LAST = SFF_SFP_TYPE_SFP28,
    SFF_SFP_TYPE_COUNT,
    SFF_SFP_TYPE_INVALID = -1,
} sff_sfp_type_t;

typedef enum sff_module_type_e {
    SFF_MODULE_TYPE_100G_AOC,
    SFF_MODULE_TYPE_100G_BASE_CR4,
    SFF_MODULE_TYPE_100G_BASE_SR4,
    SFF_MODULE_TYPE_100G_BASE_LR4,
    SFF_MODULE_TYPE_100G_CWDM4,
    SFF_MODULE_TYPE_100G_PSM4,
    SFF_MODULE_TYPE_100G_SWDM4,
    SFF_MODULE_TYPE_40G_BASE_CR4,
    SFF_MODULE_TYPE_40G_BASE_SR4,
    SFF_MODULE_TYPE_40G_BASE_LR4,
    SFF_MODULE_TYPE_40G_BASE_LM4,
    SFF_MODULE_TYPE_40G_BASE_ACTIVE,
    SFF_MODULE_TYPE_40G_BASE_CR,
    SFF_MODULE_TYPE_40G_BASE_SR2,
    SFF_MODULE_TYPE_40G_BASE_SM4,
    SFF_MODULE_TYPE_40G_BASE_ER4,
    SFF_MODULE_TYPE_25G_BASE_CR,
    SFF_MODULE_TYPE_25G_BASE_SR,
    SFF_MODULE_TYPE_25G_BASE_LR,
    SFF_MODULE_TYPE_25G_BASE_AOC,
    SFF_MODULE_TYPE_10G_BASE_SR,
    SFF_MODULE_TYPE_10G_BASE_LR,
    SFF_MODULE_TYPE_10G_BASE_LRM,
    SFF_MODULE_TYPE_10G_BASE_ER,
    SFF_MODULE_TYPE_10G_BASE_CR,
    SFF_MODULE_TYPE_10G_BASE_SX,
    SFF_MODULE_TYPE_10G_BASE_LX,
    SFF_MODULE_TYPE_10G_BASE_ZR,
    SFF_MODULE_TYPE_10G_BASE_SRL,
    SFF_MODULE_TYPE_1G_BASE_SX,
    SFF_MODULE_TYPE_1G_BASE_LX,
    SFF_MODULE_TYPE_1G_BASE_ZX,
    SFF_MODULE_TYPE_1G_BASE_CX,
    SFF_MODULE_TYPE_1G_BASE_T,
    SFF_MODULE_TYPE_100_BASE_LX,
    SFF_MODULE_TYPE_100_BASE_FX,
    SFF_MODULE_TYPE_4X_MUX,
    SFF_MODULE_TYPE_LAST = SFF_MODULE_TYPE_4X_MUX,
    SFF_MODULE_TYPE_COUNT,
    SFF_MODULE_TYPE_INVALID = -1,
} sff_module_type_t;

typedef enum sff_media_type_e {
    SFF_MEDIA_TYPE_COPPER,
    SFF_MEDIA_TYPE_FIBER,
    SFF_MEDIA_TYPE_LAST = SFF_MEDIA_TYPE_FIBER,
    SFF_MEDIA_TYPE_COUNT,
    SFF_MEDIA_TYPE_INVALID = -1,
} sff_media_type_t;

typedef enum sff_module_caps_e {
    SFF_MODULE_CAPS_F_100 = 1,
    SFF_MODULE_CAPS_F_1G = 2,
    SFF_MODULE_CAPS_F_10G = 4,
    SFF_MODULE_CAPS_F_25G = 8,
    SFF_MODULE_CAPS_F_40G = 16,
    SFF_MODULE_CAPS_F_100G = 32,
} sff_module_caps_t;

typedef struct sff_info_s {
    /** Vendor Name */
    char vendor[17];
    /** Model Number */
    char model[17];
    /** Serial Number */
    char serial[17];
    /** SFP Type */
    sff_sfp_type_t sfp_type;
    /** SFP Type Name */
    const char* sfp_type_name;
    /** Module Type */
    sff_module_type_t module_type;
    /** Module Type Name */
    const char* module_type_name;
    /** Media Type */
    sff_media_type_t media_type;
    /** Media Type Name */
    const char* media_type_name;
    /** Capabilities */
    sff_module_caps_t caps;
    /** Cable length, if available */
    int length;
    /** Cable length description. */
    char length_desc[16];
} sff_info_t;
//

// From AIM/aim_bitmap.h
typedef uint32_t aim_bitmap_word_t;

typedef struct aim_bitmap_hdr_s {
    int wordcount;
    aim_bitmap_word_t* words;
    int maxbit;
    int allocated;
} aim_bitmap_hdr_t;

//#define AIM_BITMAP_BITS_PER_WORD (sizeof(aim_bitmap_word_t)*8)
#define AIM_BITMAP_BITS_PER_WORD (4*8)
#define AIM_BITMAP_WORD_COUNT    8

typedef struct aim_bitmap256_s {
  aim_bitmap_hdr_t hdr;
  aim_bitmap_word_t words[AIM_BITMAP_WORD_COUNT];
} aim_bitmap256_t;
//

// From onlp/sfp.h
#define ONLP_SFP_BLOCK_DATA_SIZE 256

typedef enum onlp_sfp_type_e {
    ONLP_SFP_TYPE_SFP,
    ONLP_SFP_TYPE_QSFP,
    ONLP_SFP_TYPE_SFP28,
    ONLP_SFP_TYPE_QSFP28,
    ONLP_SFP_TYPE_LAST = ONLP_SFP_TYPE_QSFP28,
    ONLP_SFP_TYPE_COUNT,
    ONLP_SFP_TYPE_INVALID = -1,
} onlp_sfp_type_t;

typedef struct onlp_sfp_info_t {
    /** OID Header */
    onlp_oid_hdr_t hdr;
    /** SFP Connector Type */
    onlp_sfp_type_t type;
    /** The SFP Control Status */
    uint32_t controls;
    sff_info_t sff;
    sff_dom_info_t dom;
    /** The raw data upon which the meta info is based. */
    struct {
        /** The last A0 data */
        uint8_t a0[ONLP_SFP_BLOCK_DATA_SIZE];

        /** The last A2 data (for SFP+ only) */
        uint8_t a2[ONLP_SFP_BLOCK_DATA_SIZE];
    } bytes;
} onlp_sfp_info_t;

typedef aim_bitmap256_t onlp_sfp_bitmap_t;
//

// From BigList/biglist.h
typedef struct biglist_s {
    /** Client data pointer. */
    void* data;
    /** next */
    struct biglist_s* next;
    /** previous */
    struct biglist_s* previous;
} biglist_t;
//
// End of section

#define ONLP_MAX_FRONT_PORT_NUM 256

using OnlpOid = onlp_oid_t;
using OnlpOidHeader = onlp_oid_hdr_t;
using SffDomInfo = sff_dom_info_t;
using SffInfo = sff_info_t;

using OnlpPortNumber = onlp_oid_t;
typedef std::bitset<ONLP_MAX_FRONT_PORT_NUM> onlp_bitmap_t;
using OnlpPresentBitmap = onlp_bitmap_t;
using SfpBitmap = onlp_sfp_bitmap_t;

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

  uint32_t GetId() const {
    return (oid_info_.id & 0xFFFFFF);
  }

  bool Present() const {
	HwState state = GetHardwareState();
	return !(state == HW_STATE_NOT_PRESENT);
    //return ONLP_OID_PRESENT(&oid_info_);
  }

  const OnlpOidHeader* GetHeader() const { return &oid_info_; }

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
    default:
      return SFP_TYPE_UNKNOWN;
    }
  }
  SfpModuleType GetSfpModuleType()const {
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
    default:
      return SFP_MODULE_CAPS_UNKNOWN;
    }
  }

  // The lifetimes of pointers returned by these functions are managed by this
  // object. The returned pointer will never be nullptr.
  const SffDomInfo* GetSffDomInfo() const { return &sfp_info_.dom; }
  ::util::StatusOr<const SffInfo*> GetSffInfo() const {
    return &sfp_info_.sff;
  }

 private:
  onlp_sfp_info_t sfp_info_;
};

// A interface for ONLP calls.
// This class wraps c-style direct ONLP calls with Google-style c++ calls that
// return ::util::Status.
class OnlpInterface {
 public:
  virtual ~OnlpInterface() {}

  // Return list of onlp oids in the system based on the type.
  virtual ::util::StatusOr<std::vector <OnlpOid>> GetOidList(
      onlp_oid_type_flag_t type) const = 0;

  // Given a OID object id, returns SFP info or failure.
  virtual ::util::StatusOr<SfpInfo> GetSfpInfo(OnlpOid oid) const = 0;

  // Given an OID, returns the OidInfo for that object (or an error if it
  // doesn't exist
  virtual ::util::StatusOr<OidInfo> GetOidInfo(OnlpOid oid) const = 0;

  // Return the presence bitmap for all SFP ports.
  virtual ::util::StatusOr<OnlpPresentBitmap> GetSfpPresenceBitmap() const = 0;

  // Get the maximum valid SFP port number.
  virtual ::util::StatusOr<OnlpPortNumber> GetSfpMaxPortNumber() const = 0;

};


// An OnlpInterface implementation that simulates the real ONLP wrapper.
// For unit testing of OnlpEventHandler and Onlphal.
class OnlpWrapper : public OnlpInterface {
 public:
  static ::util::StatusOr<std::unique_ptr<OnlpWrapper>> Make() {
    LOG(INFO) << "Initializing ONLP.";
    std::unique_ptr<OnlpWrapper> wrapper(new OnlpWrapper());
    return std::move(wrapper);
  }

  OnlpWrapper(const OnlpWrapper& other) = delete;
  OnlpWrapper& operator=(const OnlpWrapper& other) = delete;
  ~OnlpWrapper() override {
	LOG(INFO) << "Deinitializing ONLP.";
  };

  // Fake implementations to get rid of compilation errors when building 
  // onlphal_test
  ::util::StatusOr<std::vector<OnlpOid>> GetOidList(
      onlp_oid_type_flag_t type) const override {
    return ::util::OkStatus(); };
#if 0 
  ::util::StatusOr<std::vector<OnlpOid>> GetOidList(
      onlp_oid_type_flag_t type) const override {

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
  };
#endif

  ::util::StatusOr<OidInfo> GetOidInfo(OnlpOid oid) const override {
    return ::util::OkStatus(); };
  ::util::StatusOr<SfpInfo> GetSfpInfo(OnlpOid oid) const override {
    return ::util::OkStatus(); };
  ::util::StatusOr<OnlpPresentBitmap> GetSfpPresenceBitmap() const override {
    return ::util::OkStatus(); };
  ::util::StatusOr<OnlpPortNumber> GetSfpMaxPortNumber() const override { 
    return 16; };
#if 0
  ::util::StatusOr<OnlpPortNumber> GetSfpMaxPortNumber() const override {
    SfpBitmap bitmap;
    onlp_sfp_bitmap_t_init(&bitmap);
    int result = onlp_sfp_bitmap_get(&bitmap);
    if(result < 0) {
      LOG(ERROR) << "Failed to get valid SFP port bitmap from ONLP.";
    }

    OnlpPortNumber port_num = ONLP_MAX_FRONT_PORT_NUM;
    int bits_per_word = 32;
    int word_count = 8;
    int i, j;
    for (i = 0; i < word_count; i ++) {
      for (j = 0; j < bits_per_word; j ++) {
        if (bitmap.words[i] & (1<<j)) {
          port_num = i * bits_per_word + j + 1;
          // Note: return here only if the valid port numbers start from
          //       port 1 and are consecutive.
          //return port_num;
        }
      }
    }

    return port_num;
  };
#endif

 private:
  OnlpWrapper() {}
};


/*
class OnlpSfpPresentMap {
 public:
  OnlpSfpPresentMap() {}
  OnlpSfpPresentMap(std::bitset<256>& map) : bitmap_(map) {}

  bool IsPresent(int port) const { 
    if (port <=0 || port > max_port_)
      return false;
    return bitmap_.test(port-1);
  }

  std::bitset<256> GetBitmap() const { return bitmap_; }
  int GetMaxPortNumber() const { return max_port_; }

  bool operator== (const OnlpSfpPresentMap& map) const {
    return bitmap_ == map.GetBitmap();
  }

 private:
  int max_port_ = 16;//256;
  std::bitset<256> bitmap_;
};
*/

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_FAKE_H_

