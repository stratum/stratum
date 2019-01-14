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

// Contains unit tests for SwitchP4cBackend.

#include "stratum/p4c_backends/fpm/switch_p4c_backend.h"

#include <stdlib.h>
#include <memory>
#include <string>

#include "base/commandlineflags.h"
#include "google/protobuf/util/message_differencer.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"
#include "p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "p4lang_p4c/frontends/p4/typeMap.h"
#include "p4lang_p4c/lib/error.h"
#include "sandblaze/p4lang/p4/config/v1/p4info.host.pb.h"
#include "sandblaze/p4lang/p4/v1/p4runtime.host.pb.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.host.pb.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/bcm/bcm_tunnel_optimizer.h"
#include "stratum/p4c_backends/common/p4c_front_mid_mock.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "stratum/p4c_backends/test/test_target_info.h"

DECLARE_string(p4_pipeline_config_text_file);
DECLARE_string(p4_pipeline_config_binary_file);
DECLARE_string(target_parser_map_file);

namespace stratum {
namespace p4c_backends {

using testing::Invoke;
using testing::Return;

// SwitchP4cBackendTest is the SwitchP4cBackend test fixture.
// The test Param string is a file name with JSON IR input.
class SwitchP4cBackendTest : public testing::TestWithParam<std::string> {
 public:
  static void SetUpTestCase() {
    TestTargetInfo::SetUpTestTargetInfo();
  }
  static void TearDownTestCase() {
    TestTargetInfo::TearDownTestTargetInfo();
  }

  // Called from testing::Invoke to return a non-zero p4c error count after
  // a certain count of error-free cycles.
  unsigned ReturnErrorAfterCount() {
    unsigned p4c_error_count = 0;
    if (no_error_cycles_ <= 0) p4c_error_count = 1;
    --no_error_cycles_;
    return p4c_error_count;
  }

 protected:
  void SetUp() override {
    const std::string kParserMapFile = "stratum/"
        "p4c_backends/fpm/map_data/standard_parser_map.pb.txt";
    FLAGS_target_parser_map_file = kParserMapFile;
    tunnel_optimizer_ = absl::make_unique<BcmTunnelOptimizer>();
    // TODO(teverman): Make NULL AnnotationMapper a mock instead.
    backend_ = absl::make_unique<SwitchP4cBackend>(
        &table_mapper_, &front_mid_mock_, nullptr, tunnel_optimizer_.get());
    no_error_cycles_ = 0;
  }

  // Sets up test IR data from the input file, which is relative to the
  // p4c_backends base directory.
  void SetUpTestIR(const std::string& test_ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestIRBaseDir =
        "stratum/p4c_backends/";
    ASSERT_TRUE(ir_helper_->GenerateTestIR(kTestIRBaseDir + test_ir_file));
  }

  unsigned GetP4cInternalErrorCount() {
    return ::errorCount();
  }

  std::unique_ptr<SwitchP4cBackend> backend_;  // Common for all tests.
  TableMapGenerator table_mapper_;
  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
  P4cFrontMidMock front_mid_mock_;
  std::unique_ptr<BcmTunnelOptimizer> tunnel_optimizer_;

  // Dummy objects to satisfy P4 object references.
  ::p4::config::v1::P4Info dummy_p4_info_;
  ::p4::v1::WriteRequest dummy_const_entries_;

  // When using ReturnErrorAfterCount to report a p4c error, provides the
  // count of error-free cycles.
  int no_error_cycles_;
};

// The first series of tests verifies error detection of bad inputs.
TEST_F(SwitchP4cBackendTest, TestNullRefMap) {
  SetUpTestIR("test/testdata/simple_vlan_stack_16.ir.json");
  backend_->Compile(*ir_helper_->ir_top_level(), dummy_const_entries_,
                    dummy_p4_info_, nullptr, ir_helper_->mid_end_typemap());
  EXPECT_NE(0, GetP4cInternalErrorCount());
}

TEST_F(SwitchP4cBackendTest, TestNullTypeMap) {
  SetUpTestIR("test/testdata/simple_vlan_stack_16.ir.json");
  backend_->Compile(*ir_helper_->ir_top_level(), dummy_const_entries_,
                    dummy_p4_info_, ir_helper_->mid_end_refmap(), nullptr);
  EXPECT_NE(0, GetP4cInternalErrorCount());
}

TEST_F(SwitchP4cBackendTest, TestNoP4Program) {
  SetUpTestIR("test/testdata/simple_vlan_stack_16.ir.json");
  // The dummy program setup below has no 'main' module.
  IR::IndexedVector<IR::Node> dummy_p4_program_param;
  std::unique_ptr<IR::P4Program> dummy_program(
      new IR::P4Program(dummy_p4_program_param));
  std::unique_ptr<IR::ToplevelBlock> dummy_top_level(
      new IR::ToplevelBlock(dummy_program.get()));
  backend_->Compile(*dummy_top_level, dummy_const_entries_, dummy_p4_info_,
                    ir_helper_->mid_end_refmap(),
                    ir_helper_->mid_end_typemap());
  EXPECT_NE(0, GetP4cInternalErrorCount());
}

TEST_F(SwitchP4cBackendTest, TestInvalidP4Info) {
  // A table ID value of 0 makes this P4Info invalid.
  ::p4::config::v1::P4Info invalid_p4_info;
  ::p4::config::v1::Table* table1 = invalid_p4_info.add_tables();
  table1->mutable_preamble()->set_id(0);
  table1->mutable_preamble()->set_name("table-with-bad-id");
  SetUpTestIR("test/testdata/simple_vlan_stack_16.ir.json");
  backend_->Compile(*ir_helper_->ir_top_level(), dummy_const_entries_,
                    invalid_p4_info, ir_helper_->mid_end_refmap(),
                    ir_helper_->mid_end_typemap());
  EXPECT_NE(0, GetP4cInternalErrorCount());
}

TEST_F(SwitchP4cBackendTest, TestInvalidP4Control) {
  SetUpTestIR("test/testdata/simple_vlan_stack_16.ir.json");
  ::p4::config::v1::P4Info p4_info;
  ASSERT_TRUE(ir_helper_->GenerateP4Info(&p4_info));

  // The P4 test IR doesn't work with the standard parser map file, but this
  // test runs effectively without any parser map input.
  FLAGS_target_parser_map_file = "no-parser-map-file";

  // This test makes the p4c error count transition from 0 to 1 in the
  // middle of the third processed control.  Each control conversion calls
  // GetErrorCount twice, so 5 cycles cause an error in the middle of the
  // third control.
  no_error_cycles_ = 5;
  EXPECT_CALL(front_mid_mock_, GetErrorCount())
      .WillRepeatedly(Invoke(this,
                             &SwitchP4cBackendTest::ReturnErrorAfterCount));
  EXPECT_CALL(front_mid_mock_, IsV1Program()).WillOnce(Return(true));
  backend_->Compile(*ir_helper_->ir_top_level(), dummy_const_entries_, p4_info,
                    ir_helper_->mid_end_refmap(),
                    ir_helper_->mid_end_typemap());
}

// This test verifies no compiler errors for valid P4_16 program and IR inputs.
TEST_P(SwitchP4cBackendTest, TestValidIR) {
  // FLAGS_test_tmpdir doesn't work because this uses gunit_main_no_google3.
  char tmpdir[] = "/tmp/SwitchP4cBackendTest.XXXXXX";
  ASSERT_TRUE(mkdtemp(tmpdir));
  const std::string outdir = tmpdir;
  FLAGS_p4_pipeline_config_text_file = outdir + "/p4_pipeline_config_test.txt";
  FLAGS_p4_pipeline_config_binary_file =
      outdir + "/p4_pipeline_config_test.bin";
  SetUpTestIR(GetParam());
  ::p4::config::v1::P4Info p4_info;
  ASSERT_TRUE(ir_helper_->GenerateP4Info(&p4_info));
  EXPECT_CALL(front_mid_mock_, IsV1Program()).WillOnce(Return(true));
  EXPECT_CALL(front_mid_mock_, GetErrorCount()).WillRepeatedly(Return(0));
  backend_->Compile(*ir_helper_->ir_top_level(), dummy_const_entries_, p4_info,
                    ir_helper_->mid_end_refmap(),
                    ir_helper_->mid_end_typemap());
  EXPECT_EQ(0, GetP4cInternalErrorCount());

  // The minimal requirement for the output pipeline config files is the
  // presence of some table_map and p4_controls data.  The P4 V1 models used
  // by this test should always have 5 control methods.  Pipeline config
  // input from text and binary files should be equivalent after accounting
  // for differences in comparing the unordered maps.
  hal::P4PipelineConfig pipeline_config_from_text;
  ASSERT_TRUE(ReadProtoFromTextFile(FLAGS_p4_pipeline_config_text_file,
                                    &pipeline_config_from_text)
                  .ok());
  EXPECT_FALSE(pipeline_config_from_text.table_map().empty());
  EXPECT_EQ(5, pipeline_config_from_text.p4_controls_size());
  hal::P4PipelineConfig pipeline_config_from_bin;
  ASSERT_TRUE(ReadProtoFromBinFile(FLAGS_p4_pipeline_config_binary_file,
                                   &pipeline_config_from_bin)
                  .ok());
  protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      protobuf::util::MessageDifferencer::AS_SET);
  EXPECT_TRUE(msg_differencer.Compare(
      pipeline_config_from_text, pipeline_config_from_bin));

  const std::string cleanup("rm -rf " + outdir);
  system(cleanup.c_str());
}

INSTANTIATE_TEST_CASE_P(
  ValidIRInputFiles,
  SwitchP4cBackendTest,
  ::testing::Values("fpm/testdata/design_doc_sample1.ir.json",
                    "fpm/testdata/middleblock_p4.ir.json",
                    "fpm/testdata/spine_p4.ir.json",
                    "fpm/testdata/tor_p4.ir.json",
                    "fpm/testdata/b4_p4.ir.json",
                    "fpm/testdata/fbr_s2_p4.ir.json",
                    "fpm/testdata/fbr_s3_p4.ir.json")
);

}  // namespace p4c_backends
}  // namespace stratum
