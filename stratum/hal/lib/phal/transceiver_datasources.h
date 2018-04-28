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


#ifndef STRATUM_HAL_LIB_PHAL_TRANSCEIVER_DATASOURCES_H_
#define STRATUM_HAL_LIB_PHAL_TRANSCEIVER_DATASOURCES_H_

#include "third_party/stratum/hal/lib/phal/fixed_layout_datasource.h"
#include "third_party/stratum/hal/lib/phal/managed_attribute.h"
#include "third_party/stratum/public/proto/hal.pb.h"
#include "third_party/absl/base/integral_types.h"
#include "third_party/absl/memory/memory.h"

namespace stratum {
namespace hal {
namespace phal {

using protobuf::EnumValueDescriptor;

// Note that some EEPROM fields may change over time (e.g. temperature), while
// others remain fixed between reads. If EEPROM reads become a bottleneck, it
// may be worthwhile to break these datasources into smaller datasources that
// only read the parts of the EEPROM that are interesting.

class LightPeakDataSource : public FixedLayoutDataSource {
 public:
  // Factory function to force shared_ptr ownership.
  static std::shared_ptr<LightPeakDataSource> Make(
      std::unique_ptr<StringSourceInterface> contents,
      CachePolicy* cache_type) {
    return std::shared_ptr<LightPeakDataSource>(
        new LightPeakDataSource(std::move(contents), cache_type));
  }

  ManagedAttribute* GetManufacturerNameAttribute() {
    return &manufacturer_name_;
  }
  ManagedAttribute* GetPartNumberAttribute() { return &part_number_; }
  ManagedAttribute* GetManufactureDateAttribute() { return &manufacture_date_; }
  ManagedAttribute* GetSerialNumberAttribute() { return &serial_number_; }

 private:
  LightPeakDataSource(std::unique_ptr<StringSourceInterface> contents,
                      CachePolicy* cache_type)
      : FixedLayoutDataSource(
            std::move(contents),
            {{"vendor_oui", new TypedField<std::string>(6, 3, true)},
              {"part_no", new TypedField<std::string>(9, 4, true)},
             {"revision_number", new TypedField<uint32>(13, 1)},
             {"date_buffer", new TypedField<std::string>(14, 2)},  // Process
             {"serial_no", new TypedField<uint32>(18, 4, true)}},
            cache_type) {}

  ::util::Status UpdateValues() override;

  TypedAttribute<std::string> manufacturer_name_ {this};
  TypedAttribute<std::string> part_number_ {this};
  TypedAttribute<uint32> manufacture_date_ {this};
  TypedAttribute<std::string> serial_number_ {this};
};

// QSFP format documentation:
//     SFF-8636 QSFP+ MSA
//         (https://ta.snia.org/kws/public/download/884/8636_028p2_clean.pdf).

class QSFPDataSource : public FixedLayoutDataSource {
 public:
  // Factory function to force shared_ptr ownership.
  static std::shared_ptr<QSFPDataSource> Make(
      std::unique_ptr<StringSourceInterface> contents, CachePolicy* cache_type);

  ManagedAttribute* GetMediaTypeAttribute();

 private:
  QSFPDataSource(std::unique_ptr<StringSourceInterface> contents,
                 CachePolicy* cache_type)
      : FixedLayoutDataSource(
            std::move(contents),
            {{"revision_compliance", new TypedField<uint32>(1, 1)},
             {"data_ready", new BitmapBooleanField(2, 0, true)},  // Inverted.
             {"rx_los_1", new BitmapBooleanField(3, 0)},
             {"rx_los_2", new BitmapBooleanField(3, 1)},
             {"rx_los_3", new BitmapBooleanField(3, 2)},
             {"rx_los_4", new BitmapBooleanField(3, 3)},
             {"tx_los_1", new BitmapBooleanField(3, 4)},
             {"tx_los_2", new BitmapBooleanField(3, 5)},
             {"tx_los_3", new BitmapBooleanField(3, 6)},
             {"tx_los_4", new BitmapBooleanField(3, 7)},
             {"tx_fault_1", new BitmapBooleanField(4, 0)},
             {"tx_fault_2", new BitmapBooleanField(4, 1)},
             {"tx_fault_3", new BitmapBooleanField(4, 2)},
             {"tx_fault_4", new BitmapBooleanField(4, 3)},
             {"tx_eq_fault_1", new BitmapBooleanField(4, 4)},
             {"tx_eq_fault_2", new BitmapBooleanField(4, 5)},
             {"tx_eq_fault_3", new BitmapBooleanField(4, 6)},
             {"tx_eq_fault_4", new BitmapBooleanField(4, 7)},
             {"rx_cdr_lol_1", new BitmapBooleanField(5, 0)},
             {"rx_cdr_lol_2", new BitmapBooleanField(5, 1)},
             {"rx_cdr_lol_3", new BitmapBooleanField(5, 2)},
             {"rx_cdr_lol_4", new BitmapBooleanField(5, 3)},
             {"tx_cdr_lol_1", new BitmapBooleanField(5, 4)},
             {"tx_cdr_lol_2", new BitmapBooleanField(5, 5)},
             {"tx_cdr_lol_3", new BitmapBooleanField(5, 6)},
             {"tx_cdr_lol_4", new BitmapBooleanField(5, 7)},
             // Each of the following following alarm field lines actually
             // represents 4 separate fields added below for brevity.
             // e.g. temperature alarm =
             //     temperature_low_warn
             //     temperature_high_warn
             //     temperature_low_alarm
             //     temperature_high_alarm
             // temperature alarm (byte 6, bits 4-7)
             // vcc alarm         (byte 7, bits 4-7)
             // rx_power_2 alarm  (byte 9, bits 0-3)
             // rx_power_1 alarm  (byte 9, bits 4-7)
             // rx_power_4 alarm  (byte 10, bits 0-3)
             // rx_power_3 alarm  (byte 10, bits 4-7)
             // tx_bias_2 alarm   (byte 11, bits 0-3)
             // tx_bias_1 alarm   (byte 11, bits 4-7)
             // tx_bias_4 alarm   (byte 12, bits 0-3)
             // tx_bias_3 alarm   (byte 12, bits 4-7)
             // tx_power_2 alarm  (byte 13, bits 0-3)
             // tx_power_1 alarm  (byte 13, bits 4-7)
             // tx_power_4 alarm  (byte 14, bits 0-3)
             // tx_power_3 alarm  (byte 14, bits 4-7)
             {"temperature", new FloatingField<double>(22, 2, true, 1.0 / 256)},
             {"vcc", new FloatingField<double>(26, 2, false, 1.0 / 10000)},
             {"rx_power_1", new FloatingField<double>(34, 2, false, 1.0 / 10)},
             {"rx_power_2", new FloatingField<double>(36, 2, false, 1.0 / 10)},
             {"rx_power_3", new FloatingField<double>(38, 2, false, 1.0 / 10)},
             {"rx_power_4", new FloatingField<double>(40, 2, false, 1.0 / 10)},
             {"tx_bias_1", new FloatingField<double>(42, 2, false, 2.0 / 1000)},
             {"tx_bias_2", new FloatingField<double>(44, 2, false, 2.0 / 1000)},
             {"tx_bias_3", new FloatingField<double>(46, 2, false, 2.0 / 1000)},
             {"tx_bias_4", new FloatingField<double>(48, 2, false, 2.0 / 1000)},
             // Note that tx_power is not defined in the qsfp spec, so these
             // values are not guaranteed to be valid.
             {"tx_power_1", new FloatingField<double>(50, 2, false, 1.0 / 10)},
             {"tx_power_2", new FloatingField<double>(52, 2, false, 1.0 / 10)},
             {"tx_power_3", new FloatingField<double>(54, 2, false, 1.0 / 10)},
             {"tx_power_4", new FloatingField<double>(56, 2, false, 1.0 / 10)},
             {"tx_disable_1", new BitmapBooleanField(86, 0)},
             {"tx_disable_2", new BitmapBooleanField(86, 1)},
             {"tx_disable_3", new BitmapBooleanField(86, 2)},
             {"tx_disable_4", new BitmapBooleanField(86, 3)},
             {"tx_rate_select_1", new UnsignedBitField(88, 0, 2)},
             {"tx_rate_select_2", new UnsignedBitField(88, 2, 2)},
             {"tx_rate_select_3", new UnsignedBitField(88, 4, 2)},
             {"tx_rate_select_4", new UnsignedBitField(88, 6, 2)},
             {"verify_page_0",
              new ValidationByteField(
                  127, {0x0},
                  "QSFP EEPROM does not have page 0 mapped to upper block.")},
             {"module_id",
              new ValidationByteField(
                  128, {0x0c /* QSFP */, 0x0d /* QSFP+ */, 0x11 /* QSFP28 */},
                  "Serial ID EEPROM is not for a QSFP/QSFP+/QSFP28.")},
             {"connector_type",
              new EnumField(130, MediaType_descriptor(),
                            {{0x07, MediaType::MEDIA_TYPE_QSFP_LR4},
                             {0x0c, MediaType::MEDIA_TYPE_QSFP_PSM4}},
                            true, MediaType::MEDIA_TYPE_UNKNOWN)},
             {"ethernet_compliance",
              new EnumField(131, MediaType_descriptor(),
                            {// (S)LR4 -> disambiguate based on connector type.
                             {0x02, MediaType::MEDIA_TYPE_QSFP_LR4},
                             {0x22, MediaType::MEDIA_TYPE_QSFP_LR4},
                             {0x04, MediaType::MEDIA_TYPE_QSFP_SR4},
                             {0x14, MediaType::MEDIA_TYPE_QSFP_SR4},
                             {0x08, MediaType::MEDIA_TYPE_QSFP_COPPER},
                             // Before revision 1.5 this indicates PSM4
                             // (google-specific). At revision >= 1.5, this
                             // indicates extended_media_type.
                             {0x80, MediaType::MEDIA_TYPE_QSFP_PSM4}},
                            true, MediaType::MEDIA_TYPE_UNKNOWN)},
             {"vendor_name", new CleanedStringField(148, 16)},
             {"part_number", new CleanedStringField(168, 16)},
             {"revision_number", new TypedField<uint32>(184, 2)},
             {"extended_media_type",
              new EnumField(
                  192, MediaType_descriptor(),
                  {
                      // Only meaningful if revision >= 1.5 and
                      // ethernet_compliance is
                      // 0x80.
                      {0x02, MediaType::MEDIA_TYPE_QSFP_CSR4},  // 100G SR4
                      {0x03,
                       MediaType::MEDIA_TYPE_QSFP_CLR4},  // 100G LR4 (standard)
                      {0x08, MediaType::MEDIA_TYPE_QSFP_CCR4},  // 100G CR4
                      {0x12, MediaType::MEDIA_TYPE_QSFP_PSM4},  // PSM4
                      {0x17,
                       MediaType::MEDIA_TYPE_QSFP_CLR4}  // 100G LR4 (standard)
                  },
                  true, MediaType::MEDIA_TYPE_UNKNOWN)},
             {"serial_number", new CleanedStringField(196, 16)},
             {"manufacture_date", new TimestampField(212, 6, "%y%m%d")}},
            cache_type) {
    AddAlarmBitFields("temperature", 6, 4);
    AddAlarmBitFields("vcc", 7, 4);
    AddAlarmBitFields("rx_power_2", 9, 0);
    AddAlarmBitFields("rx_power_1", 9, 4);
    AddAlarmBitFields("rx_power_4", 10, 0);
    AddAlarmBitFields("rx_power_3", 10, 4);
    AddAlarmBitFields("tx_bias_2", 11, 0);
    AddAlarmBitFields("tx_bias_1", 11, 4);
    AddAlarmBitFields("tx_bias_4", 12, 0);
    AddAlarmBitFields("tx_bias_3", 12, 4);
    AddAlarmBitFields("tx_power_2", 13, 0);
    AddAlarmBitFields("tx_power_1", 13, 4);
    AddAlarmBitFields("tx_power_4", 14, 0);
    AddAlarmBitFields("tx_power_3", 14, 4);
  }

  ::util::Status UpdateValues() override;

  // Helper function to avoid lots of repetitive BitmapBooleanField entries.
  void AddAlarmBitFields(const std::string& prefix, size_t byte,
                         size_t low_bit);

  EnumAttribute media_type_ {MediaType_descriptor(), this};
};

// SFP format documentation:
//     SFP MSA  (http://www.schelto.com/SFP/SFP%20MSA.pdf)
//     SFF-8472 (ftp://ftp.seagate.com/sff/SFF-8472.PDF)

class SFPDataSource : public FixedLayoutDataSource {
 public:
  // Factory function to force shared_ptr ownership.
  static std::shared_ptr<SFPDataSource> Make(
      std::unique_ptr<StringSourceInterface> contents,
      CachePolicy* cache_type) {
    return std::shared_ptr<SFPDataSource>(
        new SFPDataSource(std::move(contents), cache_type));
  }

 protected:
  SFPDataSource(std::unique_ptr<StringSourceInterface> contents,
                CachePolicy* cache_type)
      : FixedLayoutDataSource(
            std::move(contents),
            {{"valid_sfp",
              new ValidationByteField(
                  0, {0x03}, "Serial ID EEPROM is not for an SFP/SFP+.")},
             {"vendor_name", new CleanedStringField(20, 16)},
             {"part_number", new CleanedStringField(40, 16)},
             {"revision_number", new TypedField<uint32>(56, 4)},
             {"serial_number", new CleanedStringField(68, 16)},
             {"manufacture_date", new TimestampField(84, 6, "%y%m%d")},
             {"temperature", new FloatingField<double>(96, 2, true, 1.0 / 256)},
             {"vcc", new FloatingField<double>(98, 2, false, 1.0 / 10000)},
             {"tx_bias", new FloatingField<double>(100, 2, false, 2.0 / 1000)},
             {"tx_power", new FloatingField<double>(102, 2, false, 1.0 / 10)},
             {"rx_power", new FloatingField<double>(104, 2, false, 1.0 / 10)},
             {"data_ready",
              new BitmapBooleanField(110, 0, true)},  // 0 == ready.
             {"rx_los", new BitmapBooleanField(110, 1)},
             {"tx_fault", new BitmapBooleanField(110, 2)},
             {"rate_select", new UnsignedBitField(110, 4, 1)},
             {"tx_disable", new BitmapBooleanField(110, 7)}},
            cache_type) {}
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TRANSCEIVER_DATASOURCES_H_
