// Contains SliceCrossReference unit tests.

#include "platforms/networking/hercules/p4c_backend/switch/slice_cross_reference.h"

#include <string>
#include <vector>

#include "platforms/networking/hercules/hal/lib/p4/p4_table_map.host.pb.h"
#include "platforms/networking/hercules/lib/utils.h"
#include "platforms/networking/hercules/p4c_backend/switch/sliced_field_map.host.pb.h"
#include "platforms/networking/hercules/p4c_backend/test/ir_test_helpers.h"
#include "platforms/networking/hercules/public/proto/p4_table_defs.host.pb.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

namespace google {
namespace hercules {
namespace p4c_backend {

// This class is the SliceCrossReference test fixture.  See the comments
// for INSTANTIATE_TEST_CASE_P at the end of the file for a description of
// the test parameters.
class SliceCrossReferenceTest
    : public testing::TestWithParam<std::tuple<std::string, std::string, int,
                                               int, int, P4FieldType, int>> {
 protected:
  // SetUpTestIR uses ir_helper_ to load an IR file in JSON format and
  // apply a ProgramInspector to record IR nodes that contain some
  // assignments with slices to test.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4File =
        "platforms/networking/hercules/p4c_backend/switch/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
  }

  // Searches the P4 test program for an action with the given name, then
  // pushes the IR node for the first statement in the action body into
  // test_assignments_.
  void GetTestAssignment(const std::string& action_name) {
    test_assignments_.clear();
    for (auto action_map_iter : ir_helper_->program_inspector().actions()) {
      const IR::P4Action* action = action_map_iter.first;
      if (std::string(action->externalName()) != action_name) continue;
      if (action->body == nullptr) break;
      if (action->body->components.empty()) break;
      test_assignments_.push_back(
          action->body->components[0]->to<IR::AssignmentStatement>());
      return;
    }
  }

  // Populates p4_pipeline_config_ with a simple field descriptor for the
  // input field.
  hal::P4FieldDescriptor* SetUpTestFieldDescriptor(
      const std::string& field_name, P4FieldType type) {
    hal::P4TableMapValue field_map_value;
    field_map_value.mutable_field_descriptor()->set_type(type);
    auto table_map = p4_pipeline_config_.mutable_table_map();
    (*table_map)[field_name] = field_map_value;
    return (*table_map)[field_name].mutable_field_descriptor();
  }

  // Updates bit offset, bit width, and header type in the selected
  // field_descriptor.
  void UpdateTestFieldDescriptor(int32 offset, int32 width,
                                 P4HeaderType header_type,
                                 hal::P4FieldDescriptor* field_descriptor) {
    field_descriptor->set_bit_offset(offset);
    field_descriptor->set_bit_width(width);
    field_descriptor->set_header_type(header_type);
  }

  // Populates sliced_field_map_ with entries to slice an Ethernet destination
  // address into an upper 32-bit slice and a lower 16-bit slice.  The field
  // types are chosen to be unique for test expectations, not to represent
  // real slices.
  void SetUpSliceMap() {
    SliceMapValue test_slices;
    SliceProperties* upper_properties = test_slices.add_slice_properties();
    upper_properties->set_slice_bit_offset(0);
    upper_properties->set_slice_bit_width(32);
    upper_properties->set_sliced_field_type(P4_FIELD_TYPE_IPV6_DST_UPPER);
    SliceProperties* lower_properties = test_slices.add_slice_properties();
    lower_properties->set_slice_bit_offset(32);
    lower_properties->set_slice_bit_width(16);
    lower_properties->set_sliced_field_type(P4_FIELD_TYPE_IPV6_DST_LOWER);
    auto mutable_slice_map = sliced_field_map_.mutable_sliced_field_map();
    (*mutable_slice_map)[P4FieldType_Name(P4_FIELD_TYPE_ETH_DST)] = test_slices;
  }

  // Test parameter accessors.
  std::string test_action_name() const { return ::testing::get<0>(GetParam()); }
  std::string destination_field_name() const {
    return ::testing::get<1>(GetParam());
  }
  int destination_field_width() const { return ::testing::get<2>(GetParam()); }
  int source_field_offset() const { return ::testing::get<3>(GetParam()); }
  int source_field_width() const { return ::testing::get<4>(GetParam()); }
  P4FieldType expected_new_field_type() const {
    return ::testing::get<5>(GetParam());
  }
  int expected_new_dest_offset() const { return ::testing::get<6>(GetParam()); }

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
  hal::P4PipelineConfig p4_pipeline_config_;     // For common test use.

  // This vector of assignments is populated by GetTestAssignment.
  std::vector<const IR::AssignmentStatement*> test_assignments_;

  // For testing of mapping larger fields into slices - empty until tests
  // populate by calling SetUpSliceMap.
  SlicedFieldMap sliced_field_map_;
};

// Tests metadata slice "meta.smaller_metadata = meta.other_metadata[31:16]",
// which should cause a field descriptor update.
TEST_F(SliceCrossReferenceTest, TestMetadataSliced) {
  SetUpTestIR("slice_assignments.ir.json");
  GetTestAssignment("ingress.assign_metadata_slice");
  auto slice_descriptor = SetUpTestFieldDescriptor(
      "meta.other_metadata", P4_FIELD_TYPE_UNKNOWN);
  auto left_descriptor = SetUpTestFieldDescriptor(
      "meta.smaller_metadata", P4_FIELD_TYPE_ETH_TYPE);
  hal::P4PipelineConfig original_pipeline_config = p4_pipeline_config_;
  hal::P4FieldDescriptor original_left = *left_descriptor;
  SliceCrossReference test_slicer(sliced_field_map_,
                                  ir_helper_->mid_end_refmap(),
                                  ir_helper_->mid_end_typemap());
  test_slicer.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  EXPECT_EQ(0, ::errorCount());
  EXPECT_FALSE(ProtoEqual(original_pipeline_config, p4_pipeline_config_));
  EXPECT_TRUE(ProtoEqual(original_left, *left_descriptor));
  EXPECT_EQ(P4_FIELD_TYPE_SLICED, slice_descriptor->type());
}

// Tests action parameter slice "meta.smaller_metadata = sliced_flags[21:6]",
// which should cause no field descriptor updates.
TEST_F(SliceCrossReferenceTest, TestParameterSliced) {
  SetUpTestIR("slice_assignments.ir.json");
  GetTestAssignment("ingress.assign_parameter_slice");
  SetUpTestFieldDescriptor("meta.smaller_metadata", P4_FIELD_TYPE_ETH_TYPE);
  hal::P4PipelineConfig original_pipeline_config = p4_pipeline_config_;
  SliceCrossReference test_slicer(sliced_field_map_,
                                  ir_helper_->mid_end_refmap(),
                                  ir_helper_->mid_end_typemap());
  test_slicer.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  EXPECT_EQ(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(original_pipeline_config, p4_pipeline_config_));
}

// Tests unsliced constant assignment "meta.color = 1", which should cause no
// field descriptor changes.
TEST_F(SliceCrossReferenceTest, TestConstantNonSliced) {
  SetUpTestIR("slice_assignments.ir.json");
  GetTestAssignment("ingress.assign_non_slice");
  SetUpTestFieldDescriptor("meta.color", P4_FIELD_TYPE_COLOR);
  hal::P4PipelineConfig original_pipeline_config = p4_pipeline_config_;

  SliceCrossReference test_slicer(sliced_field_map_,
                                  ir_helper_->mid_end_refmap(),
                                  ir_helper_->mid_end_typemap());
  test_slicer.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  EXPECT_EQ(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(original_pipeline_config, p4_pipeline_config_));
}

// Tests metadata slice "meta.smaller_metadata = meta.other_metadata[31:16]",
// where the destination field type is unknown.
TEST_F(SliceCrossReferenceTest, TestMetadataSlicedDestUnknownType) {
  SetUpTestIR("slice_assignments.ir.json");
  GetTestAssignment("ingress.assign_metadata_slice");
  SetUpTestFieldDescriptor("meta.other_metadata", P4_FIELD_TYPE_UNKNOWN);
  SetUpTestFieldDescriptor("meta.smaller_metadata", P4_FIELD_TYPE_UNKNOWN);
  hal::P4PipelineConfig original_pipeline_config = p4_pipeline_config_;
  SliceCrossReference test_slicer(sliced_field_map_,
                                  ir_helper_->mid_end_refmap(),
                                  ir_helper_->mid_end_typemap());
  test_slicer.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  EXPECT_EQ(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(original_pipeline_config, p4_pipeline_config_));
}

// Tests metadata slice "meta.smaller_metadata = meta.other_metadata[31:16]",
// where the destination field has no descriptor.
TEST_F(SliceCrossReferenceTest, TestMetadataSlicedDestNoDescriptor) {
  SetUpTestIR("slice_assignments.ir.json");
  GetTestAssignment("ingress.assign_metadata_slice");
  SetUpTestFieldDescriptor("meta.other_metadata", P4_FIELD_TYPE_UNKNOWN);
  hal::P4PipelineConfig original_pipeline_config = p4_pipeline_config_;
  SliceCrossReference test_slicer(sliced_field_map_,
                                  ir_helper_->mid_end_refmap(),
                                  ir_helper_->mid_end_typemap());
  test_slicer.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  EXPECT_EQ(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(original_pipeline_config, p4_pipeline_config_));
}

// Tests metadata slice "meta.smaller_metadata = meta.other_metadata[31:16]",
// where the source field type is already known.
TEST_F(SliceCrossReferenceTest, TestMetadataSlicedSourceKnownType) {
  SetUpTestIR("slice_assignments.ir.json");
  GetTestAssignment("ingress.assign_metadata_slice");
  SetUpTestFieldDescriptor("meta.other_metadata", P4_FIELD_TYPE_ETH_TYPE);
  SetUpTestFieldDescriptor("meta.smaller_metadata", P4_FIELD_TYPE_ETH_TYPE);
  hal::P4PipelineConfig original_pipeline_config = p4_pipeline_config_;
  SliceCrossReference test_slicer(sliced_field_map_,
                                  ir_helper_->mid_end_refmap(),
                                  ir_helper_->mid_end_typemap());
  test_slicer.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  EXPECT_EQ(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(original_pipeline_config, p4_pipeline_config_));
}

// Tests metadata slice "meta.smaller_metadata = meta.other_metadata[31:16]",
// where the source field type has no descriptor.
TEST_F(SliceCrossReferenceTest, TestMetadataSlicedSourceNoDescriptor) {
  SetUpTestIR("slice_assignments.ir.json");
  GetTestAssignment("ingress.assign_metadata_slice");
  SetUpTestFieldDescriptor("meta.smaller_metadata", P4_FIELD_TYPE_ETH_TYPE);
  hal::P4PipelineConfig original_pipeline_config = p4_pipeline_config_;
  SliceCrossReference test_slicer(sliced_field_map_,
                                  ir_helper_->mid_end_refmap(),
                                  ir_helper_->mid_end_typemap());
  test_slicer.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  EXPECT_EQ(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(original_pipeline_config, p4_pipeline_config_));
}

// Tests assignment of the sliced upper portion of a known field type to
// an unknown field type - missing slice map entry for source field type.
TEST_F(SliceCrossReferenceTest, TestSlicedUpperToUnknownNoSliceMap) {
  // SetUpSliceMap() is not called, leaving sliced_field_map_ empty.
  SetUpTestIR("slice_assignments.ir.json");
  GetTestAssignment("ingress.assign_field_slice_upper");
  auto dest_descriptor =
      SetUpTestFieldDescriptor("meta.other_metadata", P4_FIELD_TYPE_UNKNOWN);
  dest_descriptor->set_bit_width(32);
  auto source_descriptor =
      SetUpTestFieldDescriptor("hdr.ethernet.dstAddr", P4_FIELD_TYPE_ETH_DST);
  // Updates bit offset = 0, width = 48.
  UpdateTestFieldDescriptor(0, 48, P4_HEADER_ETHERNET, source_descriptor);
  hal::P4PipelineConfig original_pipeline_config = p4_pipeline_config_;
  SliceCrossReference test_slicer(sliced_field_map_,
                                  ir_helper_->mid_end_refmap(),
                                  ir_helper_->mid_end_typemap());
  test_slicer.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(original_pipeline_config, p4_pipeline_config_));
}

// Tests assignment of the sliced upper portion of a known field type to
// an unknown field type - no matching slice map entry.
TEST_F(SliceCrossReferenceTest, TestSlicedUpperToUnknownSliceMapMismatch) {
  SetUpTestIR("slice_assignments.ir.json");
  GetTestAssignment("ingress.assign_field_slice_no_match");
  SetUpSliceMap();
  auto dest_descriptor =
      SetUpTestFieldDescriptor("meta.smaller_metadata", P4_FIELD_TYPE_UNKNOWN);
  dest_descriptor->set_bit_width(32);
  auto source_descriptor =
      SetUpTestFieldDescriptor("hdr.ethernet.dstAddr", P4_FIELD_TYPE_ETH_DST);
  // Updates bit offset = 0, width = 48.
  UpdateTestFieldDescriptor(0, 48, P4_HEADER_ETHERNET, source_descriptor);
  hal::P4PipelineConfig original_pipeline_config = p4_pipeline_config_;
  SliceCrossReference test_slicer(sliced_field_map_,
                                  ir_helper_->mid_end_refmap(),
                                  ir_helper_->mid_end_typemap());
  test_slicer.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(original_pipeline_config, p4_pipeline_config_));
}

// Expects SliceCrossReference to ignore slice-to-slice assignments.
TEST_F(SliceCrossReferenceTest, TestSliceToSlice) {
  SetUpTestIR("slice_assignments.ir.json");
  GetTestAssignment("ingress.assign_field_slice_to_field_slice");
  SetUpSliceMap();
  auto dest_descriptor =
      SetUpTestFieldDescriptor("meta.other_metadata", P4_FIELD_TYPE_UNKNOWN);
  dest_descriptor->set_bit_width(32);
  auto source_descriptor =
      SetUpTestFieldDescriptor("hdr.ethernet.dstAddr", P4_FIELD_TYPE_ETH_DST);
  // Updates bit offset = 0, width = 48.
  UpdateTestFieldDescriptor(0, 48, P4_HEADER_ETHERNET, source_descriptor);
  hal::P4PipelineConfig original_pipeline_config = p4_pipeline_config_;
  SliceCrossReference test_slicer(sliced_field_map_,
                                  ir_helper_->mid_end_refmap(),
                                  ir_helper_->mid_end_typemap());
  test_slicer.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  EXPECT_EQ(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(original_pipeline_config, p4_pipeline_config_));
}

// Tests various parameterized assignments of a sliced source field with
// known field type to an unknown destination field type.
TEST_P(SliceCrossReferenceTest, TestSlicedSourceToUnknownDest) {
  SetUpTestIR("slice_assignments.ir.json");
  GetTestAssignment(test_action_name());
  SetUpSliceMap();
  auto dest_descriptor =
      SetUpTestFieldDescriptor(destination_field_name(), P4_FIELD_TYPE_UNKNOWN);
  dest_descriptor->set_bit_width(destination_field_width());
  auto source_descriptor =
      SetUpTestFieldDescriptor("hdr.ethernet.dstAddr", P4_FIELD_TYPE_ETH_DST);
  UpdateTestFieldDescriptor(source_field_offset(), source_field_width(),
                            P4_HEADER_ETHERNET, source_descriptor);
  hal::P4PipelineConfig original_pipeline_config = p4_pipeline_config_;
  SliceCrossReference test_slicer(sliced_field_map_,
                                  ir_helper_->mid_end_refmap(),
                                  ir_helper_->mid_end_typemap());
  test_slicer.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  EXPECT_EQ(0, ::errorCount());
  EXPECT_FALSE(ProtoEqual(original_pipeline_config, p4_pipeline_config_));
  EXPECT_EQ(expected_new_field_type(), dest_descriptor->type());
  EXPECT_EQ(destination_field_width(), dest_descriptor->bit_width());
  EXPECT_EQ(expected_new_dest_offset(), dest_descriptor->bit_offset());
  EXPECT_EQ(P4_HEADER_ETHERNET, dest_descriptor->header_type());
}

// Parameterized tests use a tuple that defines values for mapping slices
// of a known source field type to an unknown destination field type.  The
// 7-tuple parameter contains:
//  - Name of the action in "slice_assignments.ir.json" to be tested.
//  - Name of the destination field in the action's assignment.
//  - Bit width of the destination field in the action's assignment.
//  - Bit offset of the assigned source field relative to its header.
//  - Bit width of the assigned source field.
//  - Expected destination field type following sliced field mapping.
//  - Expected bit offset in destination field, as adjusted for source slice.
INSTANTIATE_TEST_CASE_P(
    TestFieldSlices, SliceCrossReferenceTest,
    ::testing::Values(
        // Upper slice of source field with no offset in header.
        std::make_tuple("ingress.assign_field_slice_upper",
                        "meta.other_metadata", 32, 0, 48,
                        P4_FIELD_TYPE_IPV6_DST_UPPER, 0),
        // Upper slice of source field with offset from start of header.
        std::make_tuple("ingress.assign_field_slice_upper",
                        "meta.other_metadata", 32, 17, 48,
                        P4_FIELD_TYPE_IPV6_DST_UPPER, 17),
        // Lower slice of source field with no offset in header.
        std::make_tuple("ingress.assign_field_slice_lower",
                        "meta.smaller_metadata", 16, 0, 48,
                        P4_FIELD_TYPE_IPV6_DST_LOWER, 32),
        // Lower slice of source field with offset from start of header.
        std::make_tuple("ingress.assign_field_slice_lower",
                        "meta.smaller_metadata", 16, 17, 48,
                        P4_FIELD_TYPE_IPV6_DST_LOWER, 49)));

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
