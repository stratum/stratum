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


#ifndef STRATUM_HAL_LIB_PHAL_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_DATASOURCE_H_

#include <memory>

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/managed_attribute.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"

namespace stratum {
namespace hal {
namespace phal {

// Important note: All DataSource implementations should use factory functions
// to ensure that DataSources are always instantiated into a shared_ptr.
// Required for correct behavior of GetSharedPointer.

// A CachePolicy determines when a a DataSource's cached values have expired.
// Each time UpdateValuesAndLock is called, a DataSource will check its
// CachePolicy to determine if CacheHasExpired. If so, it will attempt to
// refresh the values of its attributes from the system, and upon success will
// call CacheUpdated to indicate to the CachePolicy that any internal timers/
// counters can be reset.
class CachePolicy {
 public:
  virtual ~CachePolicy() = default;
  // Returns true if the cache has expired and should be refreshed.
  virtual bool CacheHasExpired() = 0;
  // CacheUpdated is called every time the cache is successfully updated.
  virtual void CacheUpdated() = 0;
};

// TODO: Add support for datasources that automatically update on a
// timer.

// A partial implementation of a datasource. Complete datasource implementations
// should derive from this class and define UpdateValues().
class DataSource : public std::enable_shared_from_this<DataSource> {
 public:
  virtual ~DataSource() = default;
  // This function may block for lock contention or I/O requests.
  // If this function returns success, all attributes managed by this
  // datasource will be safe to access until Unlock is called. If this
  // function returns any failure, managed attributes may not be safe
  // to access but Unlock must still be called.
  virtual ::util::Status UpdateValuesAndLock()
      EXCLUSIVE_LOCK_FUNCTION(data_lock_);
  virtual void Unlock() UNLOCK_FUNCTION(data_lock_);
  // This function may block for lock contention or I/O requests.
  // If this function returns success, any pending writes to attributes managed
  // by this datasource have been succesfully written to the system.
  virtual ::util::Status LockAndFlushWrites();
  // Returns a shared_ptr to this DataSource. May only be called on a DataSource
  // that is already held by a shared_ptr. We use this to acquire partial
  // ownership of this datasource via one of its attributes. Note that an
  // attribute cannot contain a shared_ptr to the datasource, since this would
  // either be a circular shared_ptr dependency (memory leaked!) or a shared_ptr
  // that points to itself (memory error!). As such, we need the ability to
  // safely acquire a shared_ptr to a datasource without the ability to pass
  // this shared_ptr around normally. std::enable_shared_from_this gives us this
  // behavior.
  virtual std::shared_ptr<DataSource> GetSharedPointer() {
    return shared_from_this();
  }

  // Updates this datasource without acquiring a lock, and skips all caching
  // behavior. This is generally unsafe, and should never be called while this
  // datasource is in use by an attribute database.
  ::util::Status UpdateValuesUnsafelyWithoutCacheOrLock() {
    return UpdateValues();
  }

 protected:
  // Construct a datasource that will use the given CachePolicy to determine
  // when to call UpdateValues(). Takes ownership of the given pointer.
  explicit DataSource(CachePolicy* cache_type);
  // The only function to be overridden by most datasource implementations.
  // Implementations should perform any necessary operations to populate each
  // managed attribute with its most up to date value.
  virtual ::util::Status UpdateValues() = 0;
  // A function to be optionally overridden by datasource implementations.
  // This function is called once on each datasource after a database write
  // operation has occurred. This should be used in cases where a datasource
  // expects to write multiple values to the system simultaneously, e.g. when
  // the RGB value of an LED.
  virtual ::util::Status FlushWrites() { return ::util::OkStatus(); }

  // Attempts to read a value of the given type from the given status or
  // attribute. This is a helper function for various datasource
  // implementations.
  template <typename T>
  ::util::StatusOr<T> ReadAttribute(
      ::util::StatusOr<ManagedAttribute*> statusor_attr) const {
    ASSIGN_OR_RETURN(ManagedAttribute* attr, std::move(statusor_attr));
    return attr->ReadValue<T>();
  }

  std::unique_ptr<CachePolicy> cache_type_;
  absl::Mutex data_lock_;
};

// The following classes provide a few different types of caching.

class TimedCache : public CachePolicy {
 public:
  explicit TimedCache(absl::Duration cache_duration);
  bool CacheHasExpired() override;
  void CacheUpdated() override;

 private:
  absl::Duration cache_duration_;
  absl::Time last_cache_time_;
};

class NoCache : public CachePolicy {
 public:
  NoCache() = default;
  bool CacheHasExpired() override { return true; }
  void CacheUpdated() override {}
};

class FetchOnce : public CachePolicy {
 public:
  FetchOnce();
  bool CacheHasExpired() override;
  void CacheUpdated() override;

 private:
  bool should_update_;
};

class NeverUpdate : public CachePolicy {
 public:
  NeverUpdate() = default;
  bool CacheHasExpired() override { return false; }
  void CacheUpdated() override {}
};

// Simple helper class to create different types of CachePolicy
class CachePolicyFactory {
  public:
    // Static helper function to create CachePolicy instances
    static ::util::StatusOr<CachePolicy*> CreateInstance(
        CachePolicyType cache_type, 
        int32 timed_cache_value=0);
};

// The following two datasources are complete implementations, provided for the
// common case where a piece of data is known during database configuration, and
// will never change value.

// A fake datasource that contains a single attribute of the given type
// with a fixed value for its entire lifetime.
template <typename T>
class FixedDataSource : public DataSource {
 public:
  // Factory function, since actual datasources must be handled by a shared_ptr.
  static std::shared_ptr<FixedDataSource<T>> Make(T value) {
    return std::shared_ptr<FixedDataSource<T>>(new FixedDataSource<T>(value));
  }
  ManagedAttribute* GetAttribute() { return &value_; }

 protected:
  explicit FixedDataSource(T value)
      : DataSource(new NeverUpdate()), value_(TypedAttribute<T>(this)) {
    value_.AssignValue(value);
  }
  ::util::Status UpdateValues() override {
    return MAKE_ERROR()
           << "UpdateValues() should not be called on a FixedDataSource";
  }

 private:
  TypedAttribute<T> value_;
};

// A FixedDataSource that makes fixed enum values less tedious to add.
class FixedEnumDataSource
    : public FixedDataSource<const google::protobuf::EnumValueDescriptor*> {
 public:
  // Factory function, since actual datasources must be handled by a shared_ptr.
  static std::shared_ptr<FixedEnumDataSource> Make(
      const google::protobuf::EnumDescriptor* type, int index) {
    return std::shared_ptr<FixedEnumDataSource>(
        new FixedEnumDataSource(type, index));
  }

 protected:
  FixedEnumDataSource(const google::protobuf::EnumDescriptor* type, int index)
      : FixedDataSource<const google::protobuf::EnumValueDescriptor*>(
            type->value(index)) {}
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_DATASOURCE_H_
