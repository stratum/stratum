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


#ifndef STRATUM_HAL_LIB_PHAL_MANAGED_ATTRIBUTE_H_
#define STRATUM_HAL_LIB_PHAL_MANAGED_ATTRIBUTE_H_

#include <functional>
#include <memory>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/lib/macros.h"
#include "google/protobuf/descriptor.h"

namespace stratum {
namespace hal {
namespace phal {

class DataSource;
// class TypedAttribute;

// A single attribute in an attribute database.
// Allows accessing the stored value, and can provide a data source if
// one exists.
// Also optionally presents a function for setting the value. This does not set
// the stored value  directly, but rather performs the appropriate action to set
// the value on the system.
// Example:
//   std::shared_ptr<ManagedAttribute> attr = ...
//   int32 value = absl::get<int32>(*attr->GetValue());
//   if (attr.CanSet())
//      attr.Set(value + 1);
class ManagedAttribute {
 public:
  virtual ~ManagedAttribute() {}
  virtual Attribute GetValue() const = 0;
  template <typename T> ::util::StatusOr<T> ReadValue() const {
    Attribute value = GetValue();
    auto typed_value = absl::get_if<T>(&value);
    CHECK_RETURN_IF_FALSE(typed_value)
        << "Attempted to read an attribute with the incorrect type.";
    return *typed_value;
  }
  // Returns the data source for this attribute if it exists. If the caller of
  // GetDataSource wants to hold this pointer, it should acquire a shared_ptr
  // instead by calling GetDataSource()->GetSharedPointer().
  virtual DataSource* GetDataSource() const = 0;
  // Returns true iff there is some system operation to set this value. Does
  // not guarantee that calling Set will succeed.
  virtual bool CanSet() const = 0;
  // Returns a failure status if the system operation to set the value fails
  // for any reason. May only be called if CanSet returns true.
  virtual ::util::Status Set(Attribute value) = 0;
};

// A single attribute of a known type, to be held internally by a data source.
// Allows setting the value directly via AssignValue.
template <typename T>
class TypedAttribute : public ManagedAttribute {
 public:
  // Does not transfer ownership of datasource.
  explicit TypedAttribute(DataSource* datasource) : datasource_(datasource) {}
  ~TypedAttribute() override {}
  Attribute GetValue() const override { return value_; }
  DataSource* GetDataSource() const override { return datasource_; }
  bool CanSet() const override { return setter_ != nullptr; }
  ::util::Status Set(Attribute value) override {
    if (setter_ == nullptr)
      return MAKE_ERROR() << "Selected attribute cannot be set.";
    if (!absl::holds_alternative<T>(value))
      return MAKE_ERROR() << "Called Set with incorrect attribute type.";
    return setter_(absl::get<T>(value));
  }
  void AddSetter(std::function<::util::Status(T value)> setter) {
    setter_ = setter;
  }
  void AssignValue(const T& value) { value_ = value; }

 protected:
  DataSource* datasource_;
  T value_{};
  std::function<::util::Status(T value)> setter_;
};

// A single attribute of a specific protobuf enum type, to be held internally
// by a data source.
class EnumAttribute
    : public TypedAttribute<const ::google::protobuf::EnumValueDescriptor*> {
 public:
  // Does not transfer ownership of datasource.
  explicit EnumAttribute(const ::google::protobuf::EnumDescriptor* descriptor,
                         DataSource* datasource)
  : TypedAttribute<const ::google::protobuf::EnumValueDescriptor*>(datasource) {
    value_ = descriptor->FindValueByNumber(0);  // Default enum value.
  }
  ::util::Status AssignValue(
    const google::protobuf::EnumValueDescriptor* value) {
    if (value->type() != value_->type()) {
      return MAKE_ERROR() << "Attempted to assign incorrect enum type "
                          << value->type()->name()
                          << " to enum attribute of type "
                          << value_->type()->name();
    }
    value_ = value;
    return ::util::OkStatus();
  }
  EnumAttribute& operator=(int number) {
    value_ = value_->type()->FindValueByNumber(number);
    return *this;
  }
  template <typename E>
  E ReadEnumValue() {
    return static_cast<E>(value_->number());
  }
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_MANAGED_ATTRIBUTE_H_
