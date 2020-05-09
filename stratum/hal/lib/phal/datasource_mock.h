// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_PHAL_DATASOURCE_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_DATASOURCE_MOCK_H_

#include <memory>

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "gmock/gmock.h"

namespace stratum {
namespace hal {
namespace phal {

class DataSourceMock : public DataSource {
 public:
  DataSourceMock() : DataSource(new NoCache()) {}
  MOCK_METHOD0(UpdateValuesAndLock, ::util::Status());
  MOCK_METHOD0(LockAndFlushWrites, :: util::Status());
  // We use DataSource's implementation of GetSharedPointer, which just calls
  // through to shared_from_this() (from std). If a mocked function returns a
  // shared_ptr to its parent mock object, adding an EXPECT_CALL will create
  // circular shared_ptr ownership and leak memory.
  void Unlock() override {}
  std::shared_ptr<DataSource> GetSharedPointer() override;

 protected:
  MOCK_METHOD0(UpdateValues, ::util::Status());
};
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_DATASOURCE_MOCK_H_
