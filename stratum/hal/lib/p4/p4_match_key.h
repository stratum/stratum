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


// A P4MatchKey class instance processes one FieldMatch entry in a P4 runtime
// TableEntry.  P4MatchKey has subclasses which handle table map conversion
// specifications for different types of matches, i.e. exact vs. ternary vs.
// longest-prefix.  The P4TableMapper uses a P4MatchKey to assist in
// mapping a match field from a P4 runtime Write RPC into a CommonFlowEntry.

#ifndef STRATUM_HAL_LIB_P4_P4_MATCH_KEY_H_
#define STRATUM_HAL_LIB_P4_P4_MATCH_KEY_H_

#include <memory>
#include <set>
#include <string>

#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/p4/common_flow_entry.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/lib/macros.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4/v1/p4runtime.pb.h"

namespace stratum {
namespace hal {

// This class is the common base class for all match keys.  The general
// P4MatchKey usage is to call CreateInstance, then call the Convert method
// to do match-type-specific conversions.
class P4MatchKey {
 public:
  // The CreateInstance factory method creates a P4MatchKey given a FieldMatch
  // from a P4 runtime request.  CreateInstance determines the appropriate
  // P4MatchKey subclass from the FieldMatch content.
  static std::unique_ptr<P4MatchKey> CreateInstance(
      const ::p4::v1::FieldMatch& p4_field_match);

  virtual ~P4MatchKey() {}

  // Converts this P4MatchKey into MappedField output within a CommonFlowEntry
  // for the match key's encapsulating WriteRequest.  The conversion_entry
  // refers to data within the P4 table map's FieldDescriptor data for the
  // match field.  Upon success, the mapped_field data contains the match key's
  // value, and possibly prefix or mask, encoded according to conversion_entry
  // and bit_width. If the conversion fails due to invalid match field data, the
  // return status contains ERR_INVALID_PARAM.  In some cases, the conversion
  // may not be supported by the implementation, so the status is
  // ERR_OPER_NOT_SUPPORTED. For any error, P4MatchKey copies the original field
  // match data into mapped_field->value().raw_pi_match().  When the status
  // indicates an error, the caller may want to use APPEND_ERROR to add
  // additional qualifying information, such as the name of the table that is
  // the target of this P4MatchKey instance.
  virtual ::util::Status Convert(
      const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
      int bit_width, MappedField* mapped_field);

  // Performs a specialized conversion of this P4MatchKey into an unsigned
  // 64-bit integer, regardless of how the match field appears in the P4Info
  // spec and the P4Runtime request.  This conversion is only possible for
  // exact-match keys where the P4Runtime encoding is less than 64 bits wide.
  // For any other type of match key, the return status contains
  // ERR_INVALID_PARAM.  This conversion option has limited usage in
  // processing certain static table entries internally within p4c.
  virtual ::util::StatusOr<uint64> ConvertExactToUint64();

  // Accessor, mainly for unit tests.
  ::p4::config::v1::MatchField::MatchType allowed_match_type() const {
    return allowed_match_type_;
  }

  // P4MatchKey and its subclasses are neither copyable nor movable.
  P4MatchKey(const P4MatchKey&) = delete;
  P4MatchKey& operator=(const P4MatchKey&) = delete;

 protected:
  // The constructor is protected, use CreateInstance instead.
  P4MatchKey(const ::p4::v1::FieldMatch& p4_field_match,
             ::p4::config::v1::MatchField::MatchType allowed_match_type);

  // Subclasses typically override ConvertValue to do match-type-specific
  // conversion.  The base class default implementation simply copies the
  // match field into mapped_field's raw_pi_match field.
  virtual ::util::Status ConvertValue(
      const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
      int bit_width, MappedField* mapped_field);

  // The remaining methods are for subclass use in converting match field
  // runtime values to a MappedField output value:
  //  ConvertBytes - converts the P4 runtime bytes_value to a MappedField::Value
  //      according to conversion_entry and bit_width specifications.  The input
  //      bytes are expected to be in network byte order.
  //  ConvertLPMPrefixLengthToMask - converts the prefix length field in a
  //      P4 runtime LPM match.  The prefix length in a P4 FieldMatch is
  //      always encoded as an integer that needs to be converted to either
  //      an integer bit mask or a series of bytes containing longer masks.
  virtual ::util::Status ConvertBytes(
      const std::string& bytes_value,
      const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
      int bit_width, MappedField::Value* mapped_value);

  ::util::Status ConvertLPMPrefixLengthToMask(
      const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
      int bit_width, MappedField::Value* mapped_value);

  // Copies the original p4_field_match_ into mapped_value's raw_pi_match field.
  void CopyRawMatchValue(MappedField::Value* mapped_value);

  // Accessor for subclasses to obtain the P4 runtime FieldMatch data.
  const ::p4::v1::FieldMatch& p4_field_match() const { return p4_field_match_; }

 private:
  // This function takes an unsigned integer encoded as string data and
  // converts it to the desired unsigned type.  The bytes in the string are
  // assumed to be in network byte order.  If the number of input bytes is too
  // large for the output type, the status contains ERR_INVALID_PARAM.
  template <typename U> ::util::Status StringDataToU(
      const std::string& bytes, int32_t bit_width, U* value);

  // This function encodes an unsigned integer containing a bit mask of the
  // specified length.
  template <typename U> U CreateUIntMask(int field_width, int mask_length);

  // This function encodes a string containing the bits in a mask of the
  // specified length.
  std::string CreateStringMask(int field_width, int mask_length);

  // Checks whether the binary-encoded value in the input string conforms to
  // the P4Info-specified bit length given by bit_width.  The implementation
  // complies with section "8.3 Bytestrings" in the "P4Runtime Specification".
  ::util::Status CheckBitWidth(const std::string& bytes_value, int bit_width);

  // This member stores the P4 FieldMatch given to CreateInstance.
  const ::p4::v1::FieldMatch p4_field_match_;

  // This member stores the subclass-dependent match type, i.e.
  // EXACT/LPM/TERNARY/RANGE.
  const ::p4::config::v1::MatchField::MatchType allowed_match_type_;
};

// P4MatchKey subclass for P4 config MatchField::EXACT.
class P4MatchKeyExact : public P4MatchKey {
 public:
  static std::unique_ptr<P4MatchKeyExact> CreateInstance(
      const ::p4::v1::FieldMatch& p4_field_match);

  ~P4MatchKeyExact() override {}

 protected:
  explicit P4MatchKeyExact(const ::p4::v1::FieldMatch& p4_field_match)
      : P4MatchKey(p4_field_match, ::p4::config::v1::MatchField::EXACT) {}

  ::util::Status ConvertValue(
      const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
      int bit_width, MappedField* mapped_field) override;
};

// P4MatchKey subclass for P4 config MatchField::TERNARY.
class P4MatchKeyTernary : public P4MatchKey {
 public:
  static std::unique_ptr<P4MatchKeyTernary> CreateInstance(
      const ::p4::v1::FieldMatch& p4_field_match);

  ~P4MatchKeyTernary() override {}

 protected:
  explicit P4MatchKeyTernary(const ::p4::v1::FieldMatch& p4_field_match)
      : P4MatchKey(p4_field_match, ::p4::config::v1::MatchField::TERNARY) {}

  ::util::Status ConvertValue(
      const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
      int bit_width, MappedField* mapped_field) override;
};

// P4MatchKey subclass for P4 config MatchField::LPM.
class P4MatchKeyLPM : public P4MatchKey {
 public:
  static std::unique_ptr<P4MatchKeyLPM> CreateInstance(
      const ::p4::v1::FieldMatch& p4_field_match);

  ~P4MatchKeyLPM() override {}

 protected:
  explicit P4MatchKeyLPM(const ::p4::v1::FieldMatch& p4_field_match)
      : P4MatchKey(p4_field_match, ::p4::config::v1::MatchField::LPM) {}

  ::util::Status ConvertValue(
      const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
      int bit_width, MappedField* mapped_field) override;
};

// P4MatchKey subclass for P4 config MatchField::RANGE.
class P4MatchKeyRange : public P4MatchKey {
 public:
  static std::unique_ptr<P4MatchKeyRange> CreateInstance(
      const ::p4::v1::FieldMatch& p4_field_match);

  ~P4MatchKeyRange() override {}

 protected:
  explicit P4MatchKeyRange(const ::p4::v1::FieldMatch& p4_field_match)
      : P4MatchKey(p4_field_match, ::p4::config::v1::MatchField::RANGE) {}

  // TODO: P4MatchKeyRange needs a ConvertValue override.
};

// P4MatchKey subclass for any FieldMatch that does not contain data for
// a field_match_type.  For certain field types, this is a valid way to
// match a default value.  For other types, it is an invalid FieldMatch.
class P4MatchKeyUnspecified : public P4MatchKey {
 public:
  static std::unique_ptr<P4MatchKeyUnspecified> CreateInstance(
      const ::p4::v1::FieldMatch& p4_field_match);

  ~P4MatchKeyUnspecified() override {}

  // The Convert override determines whether this P4MatchKey's field is a
  // valid default value based on the input conversion_entry.
  ::util::Status Convert(
      const P4FieldDescriptor::P4FieldConversionEntry& conversion_entry,
      int bit_width, MappedField* mapped_field) override;

 protected:
  explicit P4MatchKeyUnspecified(const ::p4::v1::FieldMatch& p4_field_match)
      : P4MatchKey(p4_field_match, ::p4::config::v1::MatchField::UNSPECIFIED) {}
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_MATCH_KEY_H_
