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


#ifndef STRATUM_HAL_LIB_PHAL_DATASOURCE_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_DATASOURCE_MOCK_H_

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "testing/base/public/gmock.h"

namespace stratum {
namespace hal {
namespace phal {

class DataSourceMock : public DataSource {
 public:
  DataSourceMock() : DataSource(new NoCache()) {}
  MOCK_METHOD0(UpdateValuesAndLock, ::util::Status());
  // We use DataSource's implementation of GetSharedPointer, which just calls
  // through to shared_from_this() (from std). If a mocked function returns a
  // shared_ptr to its parent mock object, adding an EXPECT_CALL will create
  // circular shared_ptr ownership and leak memory.
  void Unlock() override {}

 protected:
  MOCK_METHOD0(UpdateValues, ::util::Status());
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_DATASOURCE_MOCK_H_
