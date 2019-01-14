// Copyright 2019 Google LLC
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

// Tests the BackendPassManager.

#include "stratum/p4c_backends/common/backend_pass_manager.h"

#include <memory>
#include "base/commandlineflags.h"
#include "stratum/p4c_backends/common/backend_extension_mock.h"
#include "stratum/p4c_backends/common/p4c_front_mid_mock.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"
#include "p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "p4lang_p4c/frontends/p4/typeMap.h"
#include "p4lang_p4c/ir/ir.h"

DECLARE_string(p4c_fe_options);

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Return;

namespace stratum {
namespace p4c_backends {

class BackendPassManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    dummy_program_ = absl::make_unique<IR::P4Program>(dummy_p4_program_param_);
    dummy_top_level_ =
        absl::make_unique<IR::ToplevelBlock>(dummy_program_.get());
    test_extensions_.push_back(&backend_mock_);
  }

  // Common mocks for the front/mid end and a backend extension.
  BackendExtensionMock backend_mock_;
  P4cFrontMidMock p4c_fe_mock_;

  // Dummy objects to satisfy the p4c_fe_mock_ calls.
  std::unique_ptr<IR::P4Program> dummy_program_;
  IR::IndexedVector<IR::Node> dummy_p4_program_param_;
  std::unique_ptr<IR::ToplevelBlock> dummy_top_level_;
  P4::ReferenceMap dummy_ref_map_;
  P4::TypeMap dummy_type_map_;

  // Vector for setting up test extensions.
  std::vector<BackendExtensionInterface*> test_extensions_;
};

// Tests normal behavior with all mocked methods reporting success.
TEST_F(BackendPassManagerTest, TestNormal) {
  FLAGS_p4c_fe_options = "--test-arg-for-p4c";
  BackendPassManager backend(&p4c_fe_mock_, test_extensions_);

  EXPECT_CALL(p4c_fe_mock_, GetErrorCount())
      .Times(AnyNumber()).WillRepeatedly(Return(0));
  EXPECT_CALL(p4c_fe_mock_, GetMidEndReferenceMap())
      .Times(AnyNumber()).WillRepeatedly(Return(&dummy_ref_map_));
  EXPECT_CALL(p4c_fe_mock_, GetMidEndTypeMap())
      .Times(AnyNumber()).WillRepeatedly(Return(&dummy_type_map_));
  EXPECT_CALL(p4c_fe_mock_, Initialize()).Times(1);
  EXPECT_CALL(p4c_fe_mock_, ProcessCommandLineOptions(2, _))
      .WillOnce(Return(0));
  EXPECT_CALL(p4c_fe_mock_, ParseP4File())
      .WillOnce(Return(dummy_program_.get()));
  EXPECT_CALL(p4c_fe_mock_, RunFrontEndPass())
      .WillOnce(Return(dummy_program_.get()));
  EXPECT_CALL(p4c_fe_mock_, GenerateP4Runtime(_, _)).Times(1);
  EXPECT_CALL(p4c_fe_mock_, RunMidEndPass())
      .WillOnce(Return(dummy_top_level_.get()));
  EXPECT_CALL(backend_mock_, Compile(_, _, _, _, _)).Times(1);
  EXPECT_EQ(0, backend.Compile());
}

// Tests behavior with undefined options for the p4c front/mid ends.
TEST_F(BackendPassManagerTest, TestNoP4cOptions) {
  FLAGS_p4c_fe_options = "";
  BackendPassManager backend(&p4c_fe_mock_, test_extensions_);

  // The backend should detect missing args before running any front or midend
  // passes or extensions.
  EXPECT_CALL(p4c_fe_mock_, Initialize()).Times(0);
  EXPECT_CALL(backend_mock_, Compile(_, _, _, _, _)).Times(0);
  EXPECT_NE(0, backend.Compile());
}

// Tests behavior with p4 file parsing error - ParseP4File returns nullptr.
TEST_F(BackendPassManagerTest, TestP4ParseErrorNullReturn) {
  FLAGS_p4c_fe_options = "arg-for-file.p4";
  BackendPassManager backend(&p4c_fe_mock_, test_extensions_);

  // No further mock calls should occur after the parser error.
  EXPECT_CALL(p4c_fe_mock_, GetErrorCount())
      .Times(AnyNumber()).WillRepeatedly(Return(0));
  EXPECT_CALL(p4c_fe_mock_, Initialize()).Times(1);
  EXPECT_CALL(p4c_fe_mock_, ProcessCommandLineOptions(2, _))
      .WillOnce(Return(0));
  EXPECT_CALL(p4c_fe_mock_, ParseP4File())
      .WillOnce(Return(nullptr));
  EXPECT_CALL(p4c_fe_mock_, RunFrontEndPass()).Times(0);
  EXPECT_CALL(p4c_fe_mock_, GenerateP4Runtime(_, _)).Times(0);
  EXPECT_CALL(p4c_fe_mock_, RunMidEndPass()).Times(0);
  EXPECT_CALL(backend_mock_, Compile(_, _, _, _, _)).Times(0);
  EXPECT_NE(0, backend.Compile());
}

// Tests behavior with p4 file parsing error - GetErrorCount returns non-zero.
TEST_F(BackendPassManagerTest, TestP4ParseErrorBadStatus) {
  FLAGS_p4c_fe_options = "arg-for-file.p4";
  BackendPassManager backend(&p4c_fe_mock_, test_extensions_);

  // No further mock calls should occur after the parser error.
  EXPECT_CALL(p4c_fe_mock_, Initialize()).Times(1);
  EXPECT_CALL(p4c_fe_mock_, ProcessCommandLineOptions(2, _))
      .WillOnce(Return(0));
  EXPECT_CALL(p4c_fe_mock_, ParseP4File())
      .WillOnce(Return(dummy_program_.get()));
  EXPECT_CALL(p4c_fe_mock_, GetErrorCount()).WillRepeatedly(Return(1));
  EXPECT_CALL(p4c_fe_mock_, RunFrontEndPass()).Times(0);
  EXPECT_CALL(p4c_fe_mock_, GenerateP4Runtime(_, _)).Times(0);
  EXPECT_CALL(p4c_fe_mock_, RunMidEndPass()).Times(0);
  EXPECT_CALL(backend_mock_, Compile(_, _, _, _, _)).Times(0);
  EXPECT_NE(0, backend.Compile());
}

// Tests behavior with frontend pass reporting failure.
TEST_F(BackendPassManagerTest, TestFrontEndFail) {
  FLAGS_p4c_fe_options = "--test-arg-for-p4c";
  BackendPassManager backend(&p4c_fe_mock_, test_extensions_);

  EXPECT_CALL(p4c_fe_mock_, GetErrorCount())
      .Times(AnyNumber()).WillRepeatedly(Return(0));
  EXPECT_CALL(p4c_fe_mock_, GetMidEndReferenceMap())
      .Times(AnyNumber()).WillRepeatedly(Return(&dummy_ref_map_));
  EXPECT_CALL(p4c_fe_mock_, GetMidEndTypeMap())
      .Times(AnyNumber()).WillRepeatedly(Return(&dummy_type_map_));
  EXPECT_CALL(p4c_fe_mock_, Initialize()).Times(1);
  EXPECT_CALL(p4c_fe_mock_, ProcessCommandLineOptions(2, _))
      .WillOnce(Return(0));
  EXPECT_CALL(p4c_fe_mock_, ParseP4File())
      .WillOnce(Return(dummy_program_.get()));
  EXPECT_CALL(p4c_fe_mock_, RunFrontEndPass()).WillOnce(Return(nullptr));
  EXPECT_CALL(p4c_fe_mock_, GenerateP4Runtime(_, _)).Times(0);
  EXPECT_CALL(p4c_fe_mock_, RunMidEndPass()).Times(0);
  EXPECT_CALL(backend_mock_, Compile(_, _, _, _, _)).Times(0);
  EXPECT_NE(0, backend.Compile());
}

// Tests behavior with midend pass reporting failure and extensions exist.
TEST_F(BackendPassManagerTest, TestMidEndFail) {
  FLAGS_p4c_fe_options = "--test-arg-for-p4c";
  BackendPassManager backend(&p4c_fe_mock_, test_extensions_);

  EXPECT_CALL(p4c_fe_mock_, GetErrorCount())
      .Times(AnyNumber()).WillRepeatedly(Return(0));
  EXPECT_CALL(p4c_fe_mock_, GetMidEndReferenceMap())
      .Times(AnyNumber()).WillRepeatedly(Return(&dummy_ref_map_));
  EXPECT_CALL(p4c_fe_mock_, GetMidEndTypeMap())
      .Times(AnyNumber()).WillRepeatedly(Return(&dummy_type_map_));
  EXPECT_CALL(p4c_fe_mock_, Initialize()).Times(1);
  EXPECT_CALL(p4c_fe_mock_, ProcessCommandLineOptions(2, _))
      .WillOnce(Return(0));
  EXPECT_CALL(p4c_fe_mock_, ParseP4File())
      .WillOnce(Return(dummy_program_.get()));
  EXPECT_CALL(p4c_fe_mock_, RunFrontEndPass())
      .WillOnce(Return(dummy_program_.get()));
  EXPECT_CALL(p4c_fe_mock_, GenerateP4Runtime(_, _)).Times(1);
  EXPECT_CALL(p4c_fe_mock_, RunMidEndPass()).WillOnce(Return(nullptr));
  EXPECT_CALL(backend_mock_, Compile(_, _, _, _, _)).Times(0);
  EXPECT_NE(0, backend.Compile());  // Error with extensions.
}

// Tests behavior with midend pass reporting failure and no extensions exist.
TEST_F(BackendPassManagerTest, TestMidEndFailNoExtensions) {
  FLAGS_p4c_fe_options = "--test-arg-for-p4c";
  BackendPassManager backend(&p4c_fe_mock_, {});  // Empty extensions vector.

  EXPECT_CALL(p4c_fe_mock_, GetErrorCount())
      .Times(AnyNumber())
      .WillRepeatedly(Return(0));
  EXPECT_CALL(p4c_fe_mock_, GetMidEndReferenceMap())
      .Times(AnyNumber())
      .WillRepeatedly(Return(&dummy_ref_map_));
  EXPECT_CALL(p4c_fe_mock_, GetMidEndTypeMap())
      .Times(AnyNumber())
      .WillRepeatedly(Return(&dummy_type_map_));
  EXPECT_CALL(p4c_fe_mock_, Initialize()).Times(1);
  EXPECT_CALL(p4c_fe_mock_, ProcessCommandLineOptions(2, _))
      .WillOnce(Return(0));
  EXPECT_CALL(p4c_fe_mock_, ParseP4File())
      .WillOnce(Return(dummy_program_.get()));
  EXPECT_CALL(p4c_fe_mock_, RunFrontEndPass())
      .WillOnce(Return(dummy_program_.get()));
  EXPECT_CALL(p4c_fe_mock_, GenerateP4Runtime(_, _)).Times(1);
  EXPECT_CALL(p4c_fe_mock_, RunMidEndPass()).WillOnce(Return(nullptr));
  EXPECT_CALL(backend_mock_, Compile(_, _, _, _, _)).Times(0);
  EXPECT_EQ(0, backend.Compile());  // No error without extensions.
}

// Tests behavior with P4Info serialization reporting failure.
TEST_F(BackendPassManagerTest, TestP4InfoFail) {
  FLAGS_p4c_fe_options = "--test-arg-for-p4c";
  BackendPassManager backend(&p4c_fe_mock_, test_extensions_);

  // The first 2 calls to GetErrorCount succeed.
  EXPECT_CALL(p4c_fe_mock_, GetErrorCount())
      .Times(2).WillRepeatedly(Return(0)).RetiresOnSaturation();
  EXPECT_CALL(p4c_fe_mock_, GetMidEndReferenceMap())
      .Times(AnyNumber()).WillRepeatedly(Return(&dummy_ref_map_));
  EXPECT_CALL(p4c_fe_mock_, GetMidEndTypeMap())
      .Times(AnyNumber()).WillRepeatedly(Return(&dummy_type_map_));
  EXPECT_CALL(p4c_fe_mock_, Initialize()).Times(1);
  EXPECT_CALL(p4c_fe_mock_, ProcessCommandLineOptions(2, _))
      .WillOnce(Return(0));
  EXPECT_CALL(p4c_fe_mock_, ParseP4File())
      .WillOnce(Return(dummy_program_.get()));
  EXPECT_CALL(p4c_fe_mock_, RunFrontEndPass())
      .WillOnce(Return(dummy_program_.get()));
  {
    // The GetErrorCount corresponding to SerializeP4Info fails.
    InSequence seq;
    EXPECT_CALL(p4c_fe_mock_, GenerateP4Runtime(_, _)).Times(1);
    EXPECT_CALL(p4c_fe_mock_, GetErrorCount()).WillOnce(Return(1));
  }
  EXPECT_CALL(p4c_fe_mock_, RunMidEndPass()).Times(0);
  EXPECT_CALL(backend_mock_, Compile(_, _, _, _, _)).Times(0);
  EXPECT_NE(0, backend.Compile());
}

// Tests behavior with backend extension bumping error count.
TEST_F(BackendPassManagerTest, TestExtensionCompile) {
  BackendPassManager backend(&p4c_fe_mock_, test_extensions_);

  // The first 4 calls to GetErrorCount succeed.
  EXPECT_CALL(p4c_fe_mock_, GetErrorCount())
      .Times(4).WillRepeatedly(Return(0)).RetiresOnSaturation();
  EXPECT_CALL(p4c_fe_mock_, GetMidEndReferenceMap())
      .Times(AnyNumber()).WillRepeatedly(Return(&dummy_ref_map_));
  EXPECT_CALL(p4c_fe_mock_, GetMidEndTypeMap())
      .Times(AnyNumber()).WillRepeatedly(Return(&dummy_type_map_));
  EXPECT_CALL(p4c_fe_mock_, Initialize()).Times(1);
  EXPECT_CALL(p4c_fe_mock_, ProcessCommandLineOptions(2, _))
      .WillOnce(Return(0));
  EXPECT_CALL(p4c_fe_mock_, ParseP4File())
      .WillOnce(Return(dummy_program_.get()));
  EXPECT_CALL(p4c_fe_mock_, RunFrontEndPass())
      .WillOnce(Return(dummy_program_.get()));
  EXPECT_CALL(p4c_fe_mock_, GenerateP4Runtime(_, _)).Times(1);
  EXPECT_CALL(p4c_fe_mock_, RunMidEndPass())
      .WillOnce(Return(dummy_top_level_.get()));
  {
    // The GetErrorCount GetErrorCount corresponding to the backend Compile
    // fails.
    InSequence seq;
    EXPECT_CALL(backend_mock_, Compile(_, _, _, _, _)).Times(1);
    EXPECT_CALL(p4c_fe_mock_, GetErrorCount()).WillOnce(Return(1));
  }
  EXPECT_NE(0, backend.Compile());
}



}  // namespace p4c_backends
}  // namespace stratum
