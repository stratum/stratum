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


// This file contains the implementation of P4MatchKey and its subclasses.

#include "stratum/hal/lib/p4/p4_match_key.h"

#include <string>

#include "gflags/gflags.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "absl/memory/memory.h"

// This flag determines whether P4MatchKey will enforce bytestring lengths
// according to P4Runtime spec section 8.3.  When the flag is disabled,
// P4MatchKey ignores bytestring lengths and simply requires that the integer
// value of the bytestring fit within the bitwidth specified by the P4Info.
// TODO: This flag anticipates getting the P4 API WG to relax the
// P4Runtime requirement in favor of better upward compatibility.  Once the
// decision is made, remove this flag and enforce the final requirement.
DEFINE_bool(enforce_bytestring_length, false,
            "Enforce P4Runtime bytestring lengths according to P4Runtime "
            "Spec section 8.3");

namespace stratum {
namespace hal {

// Creates a sub-class instance according to p4_field_match's type.
std::unique_ptr<P4MatchKey> P4MatchKey::CreateInstance(
    const ::p4::v1::FieldMatch& p4_field_match) {
  switch (p4_field_match.field_match_type_case()) {
    case ::p4::v1::FieldMatch::kExact:
      return P4MatchKeyExact::CreateInstance(p4_field_match);
    case ::p4::v1::FieldMatch::kTernary:
      return P4MatchKeyTernary::CreateInstance(p4_field_match);
    case ::p4::v1::FieldMatch::kLpm:
      return P4MatchKeyLPM::CreateInstance(p4_field_match);
    case ::p4::v1::FieldMatch::kRange:
      return P4MatchKeyRange::CreateInstance(p4_field_match);
    case ::p4::v1::FieldMatch::FIELD_MATCH_TYPE_NOT_SET:
      // A FieldMatch that does not set a match value of any type is
      // a valid default setting for some fields and invalid for other fields.
      // The P4MatchKeyUnspecified::Convert method figures this out when it
      // runs.
      return P4MatchKeyUnspecified::CreateInstance(p4_field_match);
    default:
      // TODO: Delete when P4Runtime removal of valid match support
      // is integrated from github.  This is only here to avoid compiler
      // complaints about "'kValid' not handled in switch".
      break;
  }
  return nullptr;
}

P4MatchKey::P4MatchKey(
    const ::p4::v1::FieldMatch& p4_field_match,
    ::p4::config::v1::MatchField::MatchType allowed_match_type)
    : p4_field_match_(p4_field_match),
      allowed_match_type_(allowed_match_type) {}

::util::Status P4MatchKey::Convert(
    const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
    int bit_width, MappedField* mapped_field) {
  if (conversion_entry.match_type() != allowed_match_type_) {
    CopyRawMatchValue(mapped_field->mutable_value());
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "P4 TableEntry match field " << p4_field_match_.ShortDebugString()
           << " cannot convert to "
           << ::p4::config::v1::MatchField_MatchType_Name(
                  conversion_entry.match_type());
  }

  ::util::Status status =
      ConvertValue(conversion_entry, bit_width, mapped_field);

  // If ConvertValue fails, the output mapped_field gets a copy of the
  // original FieldMatch data.
  if (!status.ok()) {
    mapped_field->mutable_value()->Clear();
    CopyRawMatchValue(mapped_field->mutable_value());
    return status;
  }

  return ::util::OkStatus();
}

::util::StatusOr<uint64> P4MatchKey::ConvertExactToUint64() {
  P4FieldDescriptor::P4FieldConversionEntry conversion_entry;
  conversion_entry.set_match_type(::p4::config::v1::MatchField::EXACT);
  conversion_entry.set_conversion(P4FieldDescriptor::P4_CONVERT_TO_U64);
  MappedField mapped_u64;
  ::util::Status status = Convert(conversion_entry, 64, &mapped_u64);
  if (!status.ok())
    return status;
  return mapped_u64.value().u64();
}

::util::Status P4MatchKey::ConvertValue(
    const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
    int /*bit_width*/, MappedField* mapped_field) {
  CopyRawMatchValue(mapped_field->mutable_value());
  return ::util::OkStatus();
}

::util::Status P4MatchKey::ConvertBytes(
    const std::string& bytes_value,
    const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
    int bit_width, MappedField::Value* mapped_value) {
  ::util::Status status = ::util::OkStatus();
  uint32_t value_32 = 0;
  uint64_t value_64 = 0;

  switch (conversion_entry.conversion()) {
    case P4FieldDescriptor::P4_CONVERT_UNKNOWN:
    default:
      status = MAKE_ERROR(ERR_INVALID_PARAM)
               << "P4 TableEntry match field "
               << p4_field_match_.ShortDebugString()
               << " unknown value conversion to "
               << P4FieldDescriptor_P4FieldValueConversion_Name(
                      conversion_entry.conversion());
      break;
    case P4FieldDescriptor::P4_CONVERT_RAW:
      CopyRawMatchValue(mapped_value);
      break;
    case P4FieldDescriptor::P4_CONVERT_TO_U32:
    case P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK:
      status = StringDataToU<uint32_t>(bytes_value, bit_width, &value_32);
      if (status.ok()) mapped_value->set_u32(value_32);
      break;
    case P4FieldDescriptor::P4_CONVERT_TO_U64:
    case P4FieldDescriptor::P4_CONVERT_TO_U64_AND_MASK:
      status = StringDataToU<uint64_t>(bytes_value, bit_width, &value_64);
      if (status.ok()) mapped_value->set_u64(value_64);
      break;
    case P4FieldDescriptor::P4_CONVERT_TO_BYTES:
    case P4FieldDescriptor::P4_CONVERT_TO_BYTES_AND_MASK:
      status = CheckBitWidth(bytes_value, bit_width);
      if (status.ok()) mapped_value->set_b(bytes_value);
      break;
  }

  return status;
}

::util::Status P4MatchKey::ConvertLPMPrefixLengthToMask(
    const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
    int bit_width, MappedField::Value* mapped_value) {
  ::util::Status status = ::util::OkStatus();
  int prefix_length = p4_field_match_.lpm().prefix_len();

  // The prefix should not be longer than the width of the match field.
  if (prefix_length > bit_width) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Match key LPM prefix length " << prefix_length << " exceeds "
           << "maximum bit width " << bit_width << " of match value";
  }

  switch (conversion_entry.conversion()) {
    case P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK:
      mapped_value->set_u32(
          CreateUIntMask<uint32_t>(bit_width, prefix_length));
      break;
    case P4FieldDescriptor::P4_CONVERT_TO_U64_AND_MASK:
      mapped_value->set_u64(
          CreateUIntMask<uint64_t>(bit_width, prefix_length));
      break;
    case P4FieldDescriptor::P4_CONVERT_TO_BYTES_AND_MASK:
      mapped_value->set_b(CreateStringMask(bit_width, prefix_length));
      break;
    default:
      // If the default executes, it implies that the table map's field
      // descriptor has not specified a way to convert the prefix.
      return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
             << "Field descriptor " << conversion_entry.ShortDebugString()
             << " does not specify how to convert prefix in LPM match";
      break;
  }

  return status;
}

void P4MatchKey::CopyRawMatchValue(MappedField::Value* mapped_value) {
  *(mapped_value->mutable_raw_pi_match()) = p4_field_match_;
}

template <typename U>
::util::Status P4MatchKey::StringDataToU(const std::string& bytes,
                                         int32_t bit_width, U* value) {
  const int kMaxWidth = sizeof(U) * 8;

  // Rules for binary byte-encoded value to unsigned integer conversion:
  //  1) The bit_width of the field can't be wider than the conversion type U,
  //     i.e. a 33-bit field can't go into a uint32.  This is potentially a
  //     table map field descriptor issue.
  //  2) The width of the encoded value can't exceed the width of the field
  //     in the P4 program.  This is an encoding error by the producer of the
  //     P4 runtime data.
  if (bit_width > kMaxWidth) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Match key bit width " << bit_width
                                         << " exceeds maximum unsigned "
                                         << "width of " << kMaxWidth;
  }

  ::util::Status status = CheckBitWidth(bytes, bit_width);
  if (!status.ok()) return status;
  *value = ByteStreamToUint<U>(bytes);
  return ::util::OkStatus();
}

template <typename U>
U P4MatchKey::CreateUIntMask(int field_width, int mask_length) {
  // Width consistency should be checked before calling.
  DCHECK_GE(field_width, mask_length)
      << "Mask length " << mask_length << " exceeds field size " << field_width;
  U mask = 0;
  for (int bit = field_width - 1; bit >= field_width - mask_length; --bit) {
    U bit_mask = 1;
    bit_mask <<= bit;
    mask |= bit_mask;
  }
  return mask;
}

std::string P4MatchKey::CreateStringMask(int field_width, int mask_length) {
  DCHECK_GE(field_width, mask_length)
      << "Mask length " << mask_length << " exceeds field size " << field_width;
  std::string mask_bytes;
  uint8_t mask_byte = 0;
  for (int bit = field_width - 1; bit >= 0; --bit) {
    mask_byte <<= 1;
    if (bit > field_width - mask_length - 1) {
      mask_byte |= 1;
    }
    if (!(bit % 8)) {
      mask_bytes.push_back(mask_byte);
      mask_byte = 0;
    }
  }
  return mask_bytes;
}

// The bit width check operates on the original string-encoded value.  In many
// cases, it would be more optimal to convert to integer first, then compare
// the integer value to the maximum possible value for the given width. However,
// this can't be done universally since some conversions never produce an
// integer output, so for simplicity all width checks are done the same way.
::util::Status P4MatchKey::CheckBitWidth(const std::string& bytes_value,
                                         int bit_width) {
  const int kBitsPerByte = 8;
  const size_t spec_bytes = (bit_width + (kBitsPerByte - 1)) / kBitsPerByte;

  // According to P4Runtime spec section 8.3, the string length must be the
  // number of bytes required to encode the match key's bit width in all cases,
  // even when leading zeroes are present.
  // TODO(teverman): Separate rules apply for fields of P4 type varbit.  These
  // fields will need additional support if Hercules P4 programs start using
  // varbit types.
  if (FLAGS_enforce_bytestring_length && spec_bytes != bytes_value.size()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Match key with " << bytes_value.size()
           << " bytes does not conform to P4Runtime-defined width of "
           << bit_width << " bits, which requires a match key field of "
           << spec_bytes << " bytes: "
           << p4_field_match_.ShortDebugString();
  }

  // If the P4Runtime client adds leading padding bytes beyond the P4-specified
  // width, they must be zeroes.
  int first_value_byte = 0;
  bool value_exceeds_bitwidth = false;
  if (bytes_value.size() > spec_bytes) {
    first_value_byte = bytes_value.size() - spec_bytes;
    const std::string zero_value(first_value_byte, 0);
    if (memcmp(zero_value.data(), bytes_value.data(), first_value_byte) != 0) {
      value_exceeds_bitwidth = true;
    }
  }

  // If the match key length is not an even byte multiple, the unused high-order
  // bits must be zeroes.
  const int first_byte_bits = bit_width % kBitsPerByte;
  if (first_byte_bits != 0 && bytes_value.size() >= spec_bytes) {
    if ((bytes_value[first_value_byte] & ((1 << first_byte_bits) - 1)) !=
        bytes_value[first_value_byte]) {
      value_exceeds_bitwidth = true;
    }
  }

  if (value_exceeds_bitwidth) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Match key value exceeds the P4Runtime-defined width of "
           << bit_width << " bits: "
           << p4_field_match_.ShortDebugString();
  }

  return ::util::OkStatus();
}

// P4MatchKey subclass implementations start here.
std::unique_ptr<P4MatchKeyExact> P4MatchKeyExact::CreateInstance(
    const ::p4::v1::FieldMatch& p4_field_match) {
  return absl::WrapUnique(new P4MatchKeyExact(p4_field_match));
}

::util::Status P4MatchKeyExact::ConvertValue(
    const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
    int bit_width, MappedField* mapped_field) {
  if (p4_field_match().exact().value().empty()) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Exact match field has no value: "
                                         << p4_field_match().ShortDebugString();
  }
  return ConvertBytes(p4_field_match().exact().value(), conversion_entry,
                      bit_width, mapped_field->mutable_value());
}

std::unique_ptr<P4MatchKeyTernary> P4MatchKeyTernary::CreateInstance(
    const ::p4::v1::FieldMatch& p4_field_match) {
  return absl::WrapUnique(new P4MatchKeyTernary(p4_field_match));
}

::util::Status P4MatchKeyTernary::ConvertValue(
    const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
    int bit_width, MappedField* mapped_field) {
  if (p4_field_match().ternary().value().empty() ||
      p4_field_match().ternary().mask().empty()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Ternary match field is missing value or mask: "
           << p4_field_match().ShortDebugString();
  }
  switch (conversion_entry.conversion()) {
    case P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK:
    case P4FieldDescriptor::P4_CONVERT_TO_U64_AND_MASK:
    case P4FieldDescriptor::P4_CONVERT_TO_BYTES_AND_MASK:
      break;
    default:
      // If the default executes, it implies that the table map's field
      // descriptor has not specified a way to convert the mask.
      return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
             << "Field descriptor " << conversion_entry.ShortDebugString()
             << " does not specify how to convert ternary mask";
      break;
  }
  ::util::Status status =
      ConvertBytes(p4_field_match().ternary().value(), conversion_entry,
                   bit_width, mapped_field->mutable_value());
  if (status.ok()) {
    status = ConvertBytes(p4_field_match().ternary().mask(), conversion_entry,
                          bit_width, mapped_field->mutable_mask());
  }

  return status;
}

std::unique_ptr<P4MatchKeyLPM> P4MatchKeyLPM::CreateInstance(
    const ::p4::v1::FieldMatch& p4_field_match) {
  return absl::WrapUnique(new P4MatchKeyLPM(p4_field_match));
}

::util::Status P4MatchKeyLPM::ConvertValue(
    const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
    int bit_width, MappedField* mapped_field) {
  if (p4_field_match().lpm().value().empty() ||
      p4_field_match().lpm().prefix_len() == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "LPM match field is missing value or prefix length: "
           << p4_field_match().ShortDebugString();
  }
  ::util::Status status = ConvertLPMPrefixLengthToMask(
      conversion_entry, bit_width, mapped_field->mutable_mask());
  if (status.ok()) {
    status = ConvertBytes(p4_field_match().lpm().value(), conversion_entry,
                          bit_width, mapped_field->mutable_value());
  }
  return status;
}

std::unique_ptr<P4MatchKeyRange> P4MatchKeyRange::CreateInstance(
    const ::p4::v1::FieldMatch& p4_field_match) {
  return absl::WrapUnique(new P4MatchKeyRange(p4_field_match));
}

std::unique_ptr<P4MatchKeyUnspecified> P4MatchKeyUnspecified::CreateInstance(
    const ::p4::v1::FieldMatch& p4_field_match) {
  return absl::WrapUnique(new P4MatchKeyUnspecified(p4_field_match));
}

::util::Status P4MatchKeyUnspecified::Convert(
    const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
    int /*bit_width*/, MappedField* mapped_field) {
  switch (conversion_entry.match_type()) {
    case ::p4::config::v1::MatchField::LPM:
    case ::p4::config::v1::MatchField::TERNARY:
    case ::p4::config::v1::MatchField::RANGE:
      // These types allow the default match to be defined by an empty value.
      // The default is communicated by not setting any value in mapped_field.
      break;
    default:
      CopyRawMatchValue(mapped_field->mutable_value());
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "P4 TableEntry match field "
             << p4_field_match().ShortDebugString() << " with P4 MatchType "
             << ::p4::config::v1::MatchField_MatchType_Name(
                    conversion_entry.match_type())
             << " has no default value";
  }

  return ::util::OkStatus();
}

}  // namespace hal
}  // namespace stratum
