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


#include "stratum/hal/lib/phal/transceiver_datasources.h"

#include <sstream>

#include "stratum/lib/macros.h"
#include "absl/time/time.h"

namespace stratum {
namespace hal {
namespace phal {

::util::Status LightPeakDataSource::UpdateValues() {
  RETURN_IF_ERROR(FixedLayoutDataSource::UpdateValues());
  ASSIGN_OR_RETURN(std::string vendor_oui,
                   ReadAttribute<std::string>(GetAttribute("vendor_oui")));
  ASSIGN_OR_RETURN(std::string part_number,
                   ReadAttribute<std::string>(GetAttribute("part_no")));
  ASSIGN_OR_RETURN(uint32 serial_number,
                   ReadAttribute<uint32>(GetAttribute("serial_no")));
  manufacturer_name_.AssignValue(vendor_oui);
  part_number_.AssignValue(part_number);
  // Handle a few known special cases for manufacturer name and part number.
  if (vendor_oui == std::string("\x00\x17\x6A", 3)) {
    manufacturer_name_.AssignValue("Avago");
    if (part_number == std::string("\x50\x32\x00\xFF", 4)) {
      part_number_.AssignValue("AFBR-75RG52Z");
    }
  } else if (vendor_oui == std::string("\x00\x26\x1F", 3)) {
    manufacturer_name_.AssignValue("SAE");
    if (part_number == std::string("\x08\x11\x70\x01", 4)) {
      part_number_.AssignValue("7120-004-01");
    }
  }

  std::ostringstream serial_number_string;
  serial_number_string << serial_number;
  serial_number_.AssignValue(serial_number_string.str());

  // date_buffer is 2 bytes, [year, week]. We convert it to a normal timestamp.
  ASSIGN_OR_RETURN(std::string date_buffer,
                   ReadAttribute<std::string>(GetAttribute("date_buffer")));
  CHECK_RETURN_IF_FALSE(date_buffer.size() == 2)
      << "Encountered an unexpected " << date_buffer.size()
      << " byte Light Peak date field.";
  uint32 year = 2000 + date_buffer[0];
  uint32 day = 7 * date_buffer[1];
  absl::Time manufacture_date =
      absl::FromDateTime(year, 1, day, 0, 0, 0, absl::UTCTimeZone());
  manufacture_date_.AssignValue(
      static_cast<uint32>(absl::ToUnixSeconds(manufacture_date)));
  return ::util::OkStatus();
}

// A revision compliance value greater than or equal to 0x5 indicates
// specification revision compliance >= 1.5. This revision changes the
// meaning of the ethernet_compliance field.
const uint32 kRevisionCompliance15 = 0x5;

std::shared_ptr<QSFPDataSource> QSFPDataSource::Make(
    std::unique_ptr<StringSourceInterface> contents, CachePolicy* cache_type) {
  return std::shared_ptr<QSFPDataSource>(
      new QSFPDataSource(std::move(contents), cache_type));
}

ManagedAttribute* QSFPDataSource::GetMediaTypeAttribute() {
  return &media_type_;
}

::util::Status QSFPDataSource::UpdateValues() {
  RETURN_IF_ERROR(FixedLayoutDataSource::UpdateValues());
  // Combine ethernet_compliance, extended_media_type, connector_type and
  // revision_compliance to find the actual QSFP MediaType.
  ASSIGN_OR_RETURN(auto ethernet_compliance,
                   ReadAttribute<const EnumValueDescriptor*>(
                       GetAttribute("ethernet_compliance")));
  ASSIGN_OR_RETURN(auto extended_media_type,
                   ReadAttribute<const EnumValueDescriptor*>(
                       GetAttribute("extended_media_type")));
  ASSIGN_OR_RETURN(auto connector_type,
                   ReadAttribute<const EnumValueDescriptor*>(
                       GetAttribute("connector_type")));
  ASSIGN_OR_RETURN(uint32 revision_compliance,
                   ReadAttribute<uint32>(GetAttribute("revision_compliance")));
  const EnumValueDescriptor* actual_media_type;
  switch (ethernet_compliance->number()) {
    case MEDIA_TYPE_QSFP_PSM4:
      if (revision_compliance < kRevisionCompliance15) {
        // For revision < 1.5, this indicates PSM4 (google-specific).
        actual_media_type = ethernet_compliance;
        break;
      } else {
        // For revision >= 1.5, this indicates extended media type (per spec).
        actual_media_type = extended_media_type;
        break;
      }
    case MEDIA_TYPE_QSFP_LR4:
      // Disambiguate this value based on connector type.
      actual_media_type = connector_type;
      break;
    default:
      actual_media_type = ethernet_compliance;
      break;
  }
  return media_type_.AssignValue(actual_media_type);
}

void QSFPDataSource::AddAlarmBitFields(const std::string& prefix, size_t byte,
                                       size_t low_bit) {
  AddField(prefix + "_low_warn", new BitmapBooleanField(byte, low_bit + 0));
  AddField(prefix + "_high_warn", new BitmapBooleanField(byte, low_bit + 1));
  AddField(prefix + "_low_alarm", new BitmapBooleanField(byte, low_bit + 2));
  AddField(prefix + "_high_alarm", new BitmapBooleanField(byte, low_bit + 3));
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
