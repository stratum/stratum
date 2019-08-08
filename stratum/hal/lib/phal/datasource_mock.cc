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

#include "stratum/hal/lib/phal/datasource_mock.h"

namespace stratum {

namespace hal {
namespace phal {
namespace {
class DataSourceMockPtr : public DataSource {
 public:
  static std::shared_ptr<DataSource> Make(DataSourceMock* datasource) {
    return std::shared_ptr<DataSource>(new DataSourceMockPtr(datasource));
  }

  ::util::Status UpdateValuesAndLock() override NO_THREAD_SAFETY_ANALYSIS {
    return datasource_->UpdateValuesAndLock();
  }

  ::util::Status LockAndFlushWrites() override NO_THREAD_SAFETY_ANALYSIS {
    return datasource_->LockAndFlushWrites();
  }

  void Unlock() override NO_THREAD_SAFETY_ANALYSIS {
    return datasource_->Unlock();
  }

 protected:
  ::util::Status UpdateValues() override { return ::util::OkStatus(); }

 private:
  explicit DataSourceMockPtr(DataSourceMock* datasource)
      : DataSource(new NoCache()), datasource_(datasource) {}
  DataSourceMock* datasource_;
};
}  // namespace

std::shared_ptr<DataSource> DataSourceMock::GetSharedPointer() {
  return DataSourceMockPtr::Make(this);
}
}  // namespace phal
}  // namespace hal

}  // namespace stratum
