// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/fixed_layout_datasource.h"

#include <algorithm>

#include "stratum/hal/lib/phal/buffer_tools.h"

namespace stratum {
namespace hal {
namespace phal {

template <>
::util::Status FloatingField<float>::UpdateAttribute(const char* buffer) {
  CHECK_RETURN_IF_FALSE(attribute_ != nullptr)
      << "Called UpdateAttribute before RegisterDataSource";
  float buffer_val;
  if (is_signed_) {
    buffer_val = static_cast<float>(
        ParseSignedIntegralBytes<int32>(buffer + offset_, length_, false));
  } else {
    buffer_val = static_cast<float>(
        ParseIntegralBytes<uint32>(buffer + offset_, length_, false));
  }
  attribute_->AssignValue(buffer_val * scale_ + increment_);
  return ::util::OkStatus();
}
template <>
::util::Status FloatingField<double>::UpdateAttribute(const char* buffer) {
  CHECK_RETURN_IF_FALSE(attribute_ != nullptr)
      << "Called UpdateAttribute before RegisterDataSource";
  double buffer_val;
  if (is_signed_) {
    buffer_val = static_cast<double>(
        ParseSignedIntegralBytes<int32>(buffer + offset_, length_, false));
  } else {
    buffer_val = static_cast<double>(
        ParseIntegralBytes<uint32>(buffer + offset_, length_, false));
  }
  attribute_->AssignValue(buffer_val * scale_ + increment_);
  return ::util::OkStatus();
}

template <>
::util::Status TypedField<int32>::UpdateAttribute(const char* buffer) {
  CHECK_RETURN_IF_FALSE(attribute_ != nullptr)
      << "Called UpdateAttribute before RegisterDataSource";
  attribute_->AssignValue(ParseSignedIntegralBytes<int32>(
      buffer + offset_, length_, little_endian_));
  return ::util::OkStatus();
}
template <>
::util::Status TypedField<int64>::UpdateAttribute(const char* buffer) {
  CHECK_RETURN_IF_FALSE(attribute_ != nullptr)
      << "Called UpdateAttribute before RegisterDataSource";
  attribute_->AssignValue(ParseSignedIntegralBytes<int64>(
      buffer + offset_, length_, little_endian_));
  return ::util::OkStatus();
}
template <>
::util::Status TypedField<uint32>::UpdateAttribute(const char* buffer) {
  CHECK_RETURN_IF_FALSE(attribute_ != nullptr)
      << "Called UpdateAttribute before RegisterDataSource";
  attribute_->AssignValue(
      ParseIntegralBytes<uint32>(buffer + offset_, length_, little_endian_));
  return ::util::OkStatus();
}
template <>
::util::Status TypedField<uint64>::UpdateAttribute(const char* buffer) {
  CHECK_RETURN_IF_FALSE(attribute_ != nullptr)
      << "Called UpdateAttribute before RegisterDataSource";
  attribute_->AssignValue(
      ParseIntegralBytes<uint64>(buffer + offset_, length_, little_endian_));
  return ::util::OkStatus();
}
template <>
::util::Status TypedField<std::string>::UpdateAttribute(const char* buffer) {
  CHECK_RETURN_IF_FALSE(attribute_ != nullptr)
      << "Called UpdateAttribute before RegisterDataSource";
  std::string contents(buffer + offset_, length_);
  if (little_endian_) std::reverse(contents.begin(), contents.end());
  attribute_->AssignValue(contents);
  return ::util::OkStatus();
}

::util::Status CleanedStringField::UpdateAttribute(const char* buffer) {
  CHECK_RETURN_IF_FALSE(attribute_ != nullptr)
      << "Called UpdateAttribute before RegisterDataSource";
  // Remove trailing whitespace and replace non-printable characters with '*'.
  size_t actual_length = length_;
  while (actual_length > 0 && isspace(buffer[offset_ + actual_length - 1]))
    actual_length--;
  std::string final_string(buffer + offset_, actual_length);
  for (auto character = final_string.begin(); character != final_string.end();
       ++character) {
    if (!isprint(*character)) {
      *character = '*';
    }
  }
  attribute_->AssignValue(final_string);
  return ::util::OkStatus();
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
