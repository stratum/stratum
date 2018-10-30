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


#include "stratum/hal/lib/phal/datasource.h"

#include "stratum/glue/status/status.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

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

void TimedCache::CacheUpdated() {
  last_cache_time_ = absl::Now();
}

FetchOnce::FetchOnce() : should_update_(true) {}

bool FetchOnce::CacheHasExpired() { return should_update_; }

void FetchOnce::CacheUpdated() { should_update_ = false; }

}  // namespace phal
}  // namespace hal
}  // namespace stratum
