// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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

#include "stratum/hal/lib/phal/phal.h"

#include <functional>
#include <vector>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {

using TransceiverEvent = PhalInterface::TransceiverEvent;

using stratum::test_utils::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

static constexpr int kMaxXcvrEventDepth = 256;

class PhalTest : public ::testing::Test {
 protected:
  void SetUp() override { phal_ = Phal::CreateSingleton(); }

  void TearDown() override { phal_->Shutdown(); }

 protected:
  Phal* phal_;
};

// TODO(max): write tests
TEST_F(PhalTest, SomeTest) {}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
