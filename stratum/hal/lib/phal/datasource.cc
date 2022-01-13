// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/datasource.h"

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "stratum/glue/status/status.h"

namespace stratum {
namespace hal {
namespace phal {

DataSource::DataSource(CachePolicy* cache_type)
    : cache_type_(absl::WrapUnique(cache_type)) {}

::util::Status DataSource::UpdateValuesAndLock() {
  data_lock_.Lock();
  if (cache_type_->CacheHasExpired()) {
    RETURN_IF_ERROR(UpdateValues());
    cache_type_->CacheUpdated();
  }
  return ::util::OkStatus();
}

void DataSource::Unlock() { data_lock_.Unlock(); }

::util::Status DataSource::LockAndFlushWrites() {
  absl::MutexLock lock(&data_lock_);
  return FlushWrites();
}

TimedCache::TimedCache(absl::Duration cache_duration)
    : cache_duration_(cache_duration) {}

bool TimedCache::CacheHasExpired() {
  auto time = absl::Now();
  // Check to see if we've waited longer than the cache duration. If the system
  // clock has moved backwards too much, we assume the cache to be invalid.
  return time - last_cache_time_ > cache_duration_ || time < last_cache_time_;
}

void TimedCache::CacheUpdated() { last_cache_time_ = absl::Now(); }

FetchOnce::FetchOnce() : should_update_(true) {}

bool FetchOnce::CacheHasExpired() { return should_update_; }

void FetchOnce::CacheUpdated() { should_update_ = false; }

// Create a new CachePolicy instance
// note: is passed to DataSource who then manages the deletion
::util::StatusOr<CachePolicy*> CachePolicyFactory::CreateInstance(
    CachePolicyConfig::CachePolicyType cache_type, int32 timed_cache_value) {
  switch (cache_type) {
    case CachePolicyConfig::NEVER_UPDATE:
      return new NeverUpdate();

    case CachePolicyConfig::FETCH_ONCE:
      return new FetchOnce();

    case CachePolicyConfig::TIMED_CACHE:
      return new TimedCache(absl::Seconds(timed_cache_value));

    case CachePolicyConfig::NO_CACHE:
      return new NoCache();

    default:
      return MAKE_ERROR(ERR_INVALID_PARAM) << "invalid cache type";
  }
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
