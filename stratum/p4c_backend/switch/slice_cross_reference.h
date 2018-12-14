// SliceCrossReference is similar in function to FieldCrossReference, except
// that it deals exclusively with the complexities of assigning an IR::Slice
// of one field to some other field.  Given these assignments:
//
//  hdr_type.field_1 = meta_type.flags_field[M:N];
//  hdr_type.field_2 = meta_type.flags_field[X:Y];
//
// SliceCrossReference looks for assignments where p4c knows the type of the
// destination field, but the type of the source field is unknown.  In these
// assignments, SliceCrossReference sets the overall meta_type.flags_field
// type to P4_FIELD_TYPE_SLICED in the field descriptor.  It then attempts
// to determine sub types for each bit slice of meta_type.flags_field
// according to the destination field types.

#ifndef PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_SLICE_CROSS_REFERENCE_H_
#define PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_SLICE_CROSS_REFERENCE_H_

#include <memory>
#include <vector>

#include "platforms/networking/hercules/hal/lib/p4/p4_pipeline_config.host.pb.h"
#include "platforms/networking/hercules/hal/lib/p4/p4_table_map.host.pb.h"
#include "platforms/networking/hercules/p4c_backend/switch/expression_inspector.h"
#include "platforms/networking/hercules/p4c_backend/switch/sliced_field_map.host.pb.h"
#include "p4lang_p4c/frontends/p4/coreLibrary.h"

namespace google {
namespace hercules {
namespace p4c_backend {

// Normal usage is to create a SliceCrossReference instance and then call
// ProcessAssignments with a vector of all the assignment statements in the
// P4 program.  SliceCrossReference expects to run once near the end of
// backend processing, after all other methods for determining field
// types have executed.
class SliceCrossReference {
 public:
  // The constructor requires a SlicedFieldMap defining valid field slices.
  // It also requires p4c's TypeMap and ReferenceMap.  It does not transfer
  // any ownership.
  SliceCrossReference(const SlicedFieldMap& sliced_field_map,
                      P4::ReferenceMap* ref_map, P4::TypeMap* type_map);
  virtual ~SliceCrossReference() {}

  // ProcessAssignments examines all of the input assignments for source field
  // slices whose type can be deduced from the destination field type.  Upon
  // finding any such assignments, ProcessAssignments updates the related field
  // descriptors in p4_pipeline_config.  To be most effective, the input vector
  // should contain all the assignments in the P4 program, which is available
  // from the ProgramInspector's assignments() accessor.  Any slice assignment
  // that SliceCrossReference is unable to decode triggers a P4 program error
  // via p4c's ErrorReporter.
  void ProcessAssignments(
      const std::vector<const IR::AssignmentStatement*>& assignments,
      hal::P4PipelineConfig* p4_pipeline_config);

  // SliceCrossReference is neither copyable nor movable.
  SliceCrossReference(const SliceCrossReference&) = delete;
  SliceCrossReference& operator=(const SliceCrossReference&) = delete;

 private:
  // These methods handle assignments where the right-hand side is an IR::Slice
  // expression.  HandleUnknownSourceType deals with source fields of unknown
  // type.  HandleUnknownDestType applies attributes of the slice's known type
  // to the unknown field type on the assignment's left-hand side.
  void HandleUnknownSourceType(hal::P4FieldDescriptor* source_field);
  bool HandleUnknownDestType(const hal::P4FieldDescriptor& source_field,
                             hal::P4FieldDescriptor* dest_field);

  // The SlicedFieldMap is injected via the constructor.
  const SlicedFieldMap& sliced_field_map_;

  // This ExpressionInspector helps decode IR::Slice types.
  std::unique_ptr<ExpressionInspector> slice_decoder_;
};

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_SLICE_CROSS_REFERENCE_H_
