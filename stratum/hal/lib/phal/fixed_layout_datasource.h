/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_PHAL_FIXED_LAYOUT_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_FIXED_LAYOUT_DATASOURCE_H_

#include <ctime>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/managed_attribute.h"
#include "stratum/hal/lib/phal/stringsource_interface.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {

// A field in a FixedLayoutDataSource (see below) that contains a single value.
// Everything in this file after FixedLayoutDataSource is various versions of
// FixedLayoutField. Note that all FixedLayoutField functions are not meant
// to be called from outside this file. See below for an example use.
class FixedLayoutField {
 public:
  virtual ~FixedLayoutField() {}
  // Returns the attribute that contains this value.
  virtual ManagedAttribute* GetAttribute() = 0;
  // Returns the minimum possible size of a buffer that contains this field.
  virtual size_t GetRequiredBufferSize() const = 0;
  // Read the given buffer and update value of the stored attribute.
  // May only be called after RegisterDataSource has been called.
  virtual ::util::Status UpdateAttribute(const char* buffer) = 0;
  // Setup the internal attribute so that GetDataSource() returns
  // datasource. This is called when a FixedLayoutField is passed to a
  // FixedLayoutDataSource, and generally should not be called elsewhere.
  virtual void RegisterDataSource(DataSource* datasource) = 0;
};

// A datasource implementation that reads a char buffer and extracts a set of
// fields from it, where each field has a fixed type, location, and length.
//
// Example:
//    std::unique_ptr<StringSourceInterface> contents = ...
//    std::map<std::string, FixedLayoutField*> fields {
//      {"validation", new ValidationByteField(
//            0, {0xAB, 0xAC}, "Failed to validate buffer.")}
//      {"name", new TypedField<std::string>(1, 20)},
//      {"is_present", new EnumField(21, PresenceEnum_descriptor(), {
//            {0x00, PresenceEnum::ABSENT},
//            {0x01, PresenceEnum::PRESENT},
//            {0x02, PresenceEnum::DISABLED}
//          })},
//      {"has_foo", new BitmapBooleanField(22, 0)},
//      {"has_bar", new BitmapBooleanField(22, 1)},
//      {"foo", new TypedField<uint32>(23, 1)},
//      {"bar", new TypedField<int32>(24, 4)}
//    };
//    auto datasource = FixedLayoutDataSource::Make(
//        contents, fields, new NoCache());
class FixedLayoutDataSource : public DataSource {
 public:
  // Factory function to create a shared_ptr to this datasource.
  // Takes full ownership of contents and all entries in fields.
  static std::shared_ptr<FixedLayoutDataSource> Make(
      std::unique_ptr<StringSourceInterface> contents,
      const std::map<std::string, FixedLayoutField*>& fields,
      CachePolicy* cache_type) {
    return std::shared_ptr<FixedLayoutDataSource>(
        new FixedLayoutDataSource(std::move(contents), fields, cache_type));
  }

  // Returns an error if the buffer is too small to fit all of the fields
  // or if updating any individual field fails (this includes checks performed
  // by ValidationByteField).
  ::util::Status UpdateValues() override {
    ASSIGN_OR_RETURN(std::string buffer, contents_->GetString());
    if (buffer.size() < required_buffer_size_)
      return MAKE_ERROR()
             << "Buffer is not large enough for all specified fields.";
    for (auto& field : fields_) {
      ::util::Status ret = field.second->UpdateAttribute(buffer.c_str());
      if (!ret.ok()) {
        return MAKE_ERROR() << "Encountered error while updating field "
                            << field.first << ".";
      }
    }
    return ::util::OkStatus();
  }

  // Read an attribute from this datasource for insertion into an attribute
  // database. The given name should match one of the keys in the fields map
  // used to construct the datasource.
  ::util::StatusOr<ManagedAttribute*> GetAttribute(const std::string& name) {
    auto field = fields_.find(name);
    if (field == fields_.end())
      return MAKE_ERROR() << "No such field defined: " << name << ".";
    return field->second->GetAttribute();
  }

 protected:
  // Add a single field to the layout, with the given name. Each call to this
  // function must have a unique name. Assumes ownership over any field
  // passed in. Derived classes may use this call to avoid unnecessarily complex
  // calls to the FixedLayoutDataSource constructor.
  void AddField(const std::string& name, FixedLayoutField* field) {
    size_t field_required_size = field->GetRequiredBufferSize();
    if (field_required_size > required_buffer_size_)
      required_buffer_size_ = field_required_size;
    field->RegisterDataSource(this);
    fields_.insert(std::make_pair(name, absl::WrapUnique(field)));
  }

  FixedLayoutDataSource(std::unique_ptr<StringSourceInterface> contents,
                        const std::map<std::string, FixedLayoutField*>& fields,
                        CachePolicy* cache_type)
      : DataSource(cache_type), contents_(std::move(contents)) {
    for (auto field : fields) {
      const std::string& field_name = field.first;
      FixedLayoutField* field_body = field.second;
      AddField(field_name, field_body);
    }
  }

  std::unique_ptr<StringSourceInterface> contents_;
  std::map<std::string, std::unique_ptr<FixedLayoutField>> fields_;
  size_t required_buffer_size_ = 0;
};

// A boolean field stored in a single bit.
class BitmapBooleanField : public FixedLayoutField {
 public:
  // offset: The offset of the byte in the buffer.
  // bit: The specific bit within this byte. Must be between 0 and 7.
  // invert: If true, flip the value of this boolean.
  // The resulting check is of the form buffer[offset] & (1 << bit)
  BitmapBooleanField(size_t offset, size_t bit, bool invert = false)
      : offset_(offset), invert_(invert) {
    DCHECK_LT(bit, size_t{8});
    bitmask_ = 0x1 << bit;
  }
  ManagedAttribute* GetAttribute() override { return attribute_.get(); }
  size_t GetRequiredBufferSize() const override { return offset_; }
  ::util::Status UpdateAttribute(const char* buffer) override {
    CHECK_RETURN_IF_FALSE(attribute_ != nullptr)
        << "Called UpdateAttribute before RegisterDataSource";
    bool value = buffer[offset_] & bitmask_;
    if (invert_)
      attribute_->AssignValue(!value);
    else
      attribute_->AssignValue(value);
    return ::util::OkStatus();
  }
  void RegisterDataSource(DataSource* datasource) override {
    attribute_ = absl::make_unique<TypedAttribute<bool>>(datasource);
  }

 private:
  size_t offset_;
  bool invert_;
  char bitmask_;
  std::unique_ptr<TypedAttribute<bool>> attribute_;
};

// An (optionally) multi-byte field that contains an integer value, but which
// should be transformed into a floating point value. Accepts template arguments
// float and double.
template <typename T>
class FloatingField : public FixedLayoutField {
 public:
  // offset: The offset of the first byte in the buffer.
  // length: The number of bytes to read from the given offset.
  // scale: The factor to use when scaling the read value to a float/double.
  // is_signed: If true, the value read from the buffer will be signed.
  // increment: The amount to add to the value after scaling. Defaults to 0.
  // Given value X read from the buffer, outputs (scale * X + increment).
  FloatingField(size_t offset, size_t length, bool is_signed, T scale,
                T increment = 0)
      : offset_(offset),
        length_(length),
        is_signed_(is_signed),
        scale_(scale),
        increment_(increment) {}
  ManagedAttribute* GetAttribute() override { return attribute_.get(); }
  size_t GetRequiredBufferSize() const override { return offset_ + length_; }
  ::util::Status UpdateAttribute(const char* buffer) override;
  void RegisterDataSource(DataSource* datasource) override {
    attribute_ = absl::make_unique<TypedAttribute<T>>(datasource);
  }

 private:
  size_t offset_;
  size_t length_;
  bool is_signed_;
  T scale_;
  T increment_;
  std::unique_ptr<TypedAttribute<T>> attribute_;
};

template <>
::util::Status FloatingField<float>::UpdateAttribute(const char* buffer);
template <>
::util::Status FloatingField<double>::UpdateAttribute(const char* buffer);

// An (optionally) multi-byte field that can take one of the types:
//    int32, int64, uint32, uint64, std::string
template <typename T>
class TypedField : public FixedLayoutField {
 public:
  // offset: The offset of the first byte in the buffer.
  // length: The number of bytes to read from this given offset.
  // little_endian: If true, reads the field LSB first. In the case of
  //                std::string, reads the *whole* string in reverse.
  TypedField(size_t offset, size_t length, bool little_endian = false)
      : offset_(offset), length_(length), little_endian_(little_endian) {}
  ManagedAttribute* GetAttribute() override { return attribute_.get(); }
  size_t GetRequiredBufferSize() const override { return offset_ + length_; }
  ::util::Status UpdateAttribute(const char* buffer) override;
  void RegisterDataSource(DataSource* datasource) override {
    attribute_ = absl::make_unique<TypedAttribute<T>>(datasource);
  }

 protected:
  size_t offset_;
  size_t length_;
  bool little_endian_;
  std::unique_ptr<TypedAttribute<T>> attribute_;
};

template <>
::util::Status TypedField<int32>::UpdateAttribute(const char* buffer);
template <>
::util::Status TypedField<int64>::UpdateAttribute(const char* buffer);
template <>
::util::Status TypedField<uint32>::UpdateAttribute(const char* buffer);
template <>
::util::Status TypedField<uint64>::UpdateAttribute(const char* buffer);
template <>
::util::Status TypedField<std::string>::UpdateAttribute(const char* buffer);

// A multi-byte field that contains a string. Any trailing whitespace is removed
// from the string, and non-printable characters are replaced with '*'. This
// should typically be used for fields that store plain text, whereas
// TypedField<std::string> should be used for any byte array fields.
class CleanedStringField : public FixedLayoutField {
 public:
  // offset: The offset of the first byte in the buffer.
  // length: The number of bytes to read from this given offset.
  CleanedStringField(size_t offset, size_t length)
      : offset_(offset), length_(length) {}
  ManagedAttribute* GetAttribute() override { return attribute_.get(); }
  size_t GetRequiredBufferSize() const override { return offset_ + length_; }
  ::util::Status UpdateAttribute(const char* buffer) override;
  void RegisterDataSource(DataSource* datasource) override {
    attribute_ = absl::make_unique<TypedAttribute<std::string>>(datasource);
  }

 protected:
  size_t offset_;
  size_t length_;
  std::unique_ptr<TypedAttribute<std::string>> attribute_;
};

// A byte field that must have one of a set of values. If this condition is
// not met, FixedLayoutDataSource::UpdateValues() will return an error.
class ValidationByteField : public TypedField<int32> {
 public:
  // offset: The offset of the byte in the buffer.
  // byte_vals: The set of values the byte might take in a well-formed buffer.
  // error_message: The error message to produce if this check fails.
  ValidationByteField(size_t offset, std::set<char> byte_vals,
                      const std::string& error_message)
      : TypedField<int32>(offset, 1),
        byte_vals_(std::move(byte_vals)),
        error_message_(error_message) {}

  ::util::Status UpdateAttribute(const char* buffer) override {
    RETURN_IF_ERROR(TypedField::UpdateAttribute(buffer));
    int32 actual_val = absl::get<int32>(attribute_->GetValue());
    if (byte_vals_.find(actual_val) == byte_vals_.end())
      return MAKE_ERROR() << error_message_;
    return ::util::OkStatus();
  }

 private:
  std::set<char> byte_vals_;
  std::string error_message_;
};

// A uint32 read from a subset of a single byte.
class UnsignedBitField : public FixedLayoutField {
 public:
  // byte_offset: The offset of the byte in the buffer.
  // bit_offset: The least significant bit in the byte that will be read.
  // length: The number of bits to read from this given offset and bit_offset.
  // E.g. for byte 0x10101100, bit_offset = 2, length = 4, we would read 0x1011.
  //                   ^^^^
  UnsignedBitField(size_t byte_offset, size_t bit_offset, size_t length)
      : byte_offset_(byte_offset), bit_offset_(bit_offset), length_(length) {
    DCHECK_LT(length, size_t{8});
  }
  ManagedAttribute* GetAttribute() override { return attribute_.get(); }
  size_t GetRequiredBufferSize() const override { return byte_offset_ + 1; }
  ::util::Status UpdateAttribute(const char* buffer) override {
    CHECK_RETURN_IF_FALSE(attribute_ != nullptr)
        << "Called UpdateAttribute before RegisterDataSource";
    uint32 value = 0;
    for (size_t i = 0; i < length_; i++) {
      value <<= 1;
      value |= (buffer[byte_offset_] >> (bit_offset_ + length_ - i - 1)) & 0x1;
    }
    attribute_->AssignValue(value);
    return ::util::OkStatus();
  }
  void RegisterDataSource(DataSource* datasource) override {
    attribute_ = absl::make_unique<TypedAttribute<uint32>>(datasource);
  }

 protected:
  size_t byte_offset_;
  int bit_offset_;
  size_t length_;
  std::unique_ptr<TypedAttribute<uint32>> attribute_;
};

// Reads an ascii time with a given format, and converts into a uint32
// timestamp.
class TimestampField : public FixedLayoutField {
 public:
  // offset: The offset of the first byte in the buffer.
  // length: The expected number of characters in the ascii timestamp.
  // format: The format string for parsing the timestamp (e.g. '%y%m%d')
  //         This should be a valid strptime format string.
  TimestampField(size_t offset, size_t length, const std::string& format)
      : offset_(offset), length_(length), format_(format) {}
  ManagedAttribute* GetAttribute() override { return attribute_.get(); }
  size_t GetRequiredBufferSize() const override { return offset_ + length_; }
  ::util::Status UpdateAttribute(const char* buffer) override {
    CHECK_RETURN_IF_FALSE(attribute_ != nullptr)
        << "Called UpdateAttribute before RegisterDataSource";
    struct tm temp_tm = {};
    // strptime should return a pointer off the end of the timestamp.
    char* after_timestamp =
        strptime(buffer + offset_, format_.c_str(), &temp_tm);
    CHECK_RETURN_IF_FALSE(after_timestamp == buffer + length_ + offset_)
        << "Failed to parse contents of timestamp field.";
    time_t timestamp = mktime(&temp_tm);
    CHECK_RETURN_IF_FALSE(timestamp != -1)
        << "Failed to convert contents of timestamp field into a timestamp.";
    attribute_->AssignValue(timestamp);
    return ::util::OkStatus();
  }
  void RegisterDataSource(DataSource* datasource) override {
    attribute_ = absl::make_unique<TypedAttribute<uint32>>(datasource);
  }

 private:
  size_t offset_;
  size_t length_;
  std::string format_;
  std::unique_ptr<TypedAttribute<uint32>> attribute_;
};

// A one byte field that produces different enum values based on different
// byte values.
class EnumField : public FixedLayoutField {
 public:
  // offset: The offset of the byte in the buffer.
  // enum_type: All possible enum values produced by this field must have this
  //            EnumDescriptor.
  // byte_to_enum_value: A mapping from the value of the byte to the enum
  //                     value produced.
  // (has_)default_value: Set a default value to use for any byte values not
  //                      explicitly defined in byte_to_enum_value.
  EnumField(size_t offset, const google::protobuf::EnumDescriptor* enum_type,
            const std::map<char, int>& byte_to_enum_value,
            bool has_default_value = false, int default_value = 0)
      : offset_(offset),
        enum_type_(enum_type),
        byte_to_enum_value_(byte_to_enum_value),
        has_default_value_(has_default_value),
        default_value_(default_value) {}
  ManagedAttribute* GetAttribute() override { return attribute_.get(); }
  size_t GetRequiredBufferSize() const override { return offset_; }
  ::util::Status UpdateAttribute(const char* buffer) override {
    CHECK_RETURN_IF_FALSE(attribute_ != nullptr)
        << "Called UpdateAttribute before RegisterDataSource";
    auto found = byte_to_enum_value_.find(buffer[offset_]);
    if (found == byte_to_enum_value_.end()) {
      if (has_default_value_) {
        *attribute_ = default_value_;
        return ::util::OkStatus();
      }
      return MAKE_ERROR() << "No enum value for byte value "
                          << static_cast<int32>(buffer[offset_]);
    }
    // TODO(unknown): Change operator= to another AssignValue.
    *attribute_ = found->second;
    return ::util::OkStatus();
  }
  void RegisterDataSource(DataSource* datasource) override {
    attribute_ = absl::make_unique<EnumAttribute>(enum_type_, datasource);
  }

 private:
  std::string name_;
  size_t offset_;
  const google::protobuf::EnumDescriptor* enum_type_;
  std::map<char, int> byte_to_enum_value_;
  bool has_default_value_;
  int default_value_;
  std::unique_ptr<EnumAttribute> attribute_;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_FIXED_LAYOUT_DATASOURCE_H_
