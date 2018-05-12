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


#ifndef STRATUM_HAL_LIB_BCM_BCM_ACL_PIPELINE_H_
#define STRATUM_HAL_LIB_BCM_BCM_ACL_PIPELINE_H_

#include <memory>
#include <vector>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/p4/common_flow_entry.pb.h"
#include "stratum/hal/lib/p4/p4_control.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "sandblaze/p4lang/p4/config/p4info.pb.h"
#include "util/gtl/flat_hash_map.h"

namespace stratum {
namespace hal {
namespace bcm {

// Contains ACL table information extracted from the p4 control flow.
struct BcmAclPipelineTable {
  BcmAclPipelineTable() : table(), priority(0) {}
  P4ControlTableRef table;
  int priority;
  // TODO: Add applied actions to this structure.
};

// AclTablePipeline repersents a forwarding pipeline in one of the ACL
// sections in the chip (i.e. VFP, IFP, EFP).
//
// Due to BCM's ACL action resolution scheme, ACL pipelines can only account
// for two kinds of table relationships:
//   1) Sequential stateless table application. Tables cannot rely on the
//      result of any previous table lookup.
//      A.apply(); B.apply(); C.apply(); ... Z.apply();
//   2) Perfectly nested "if missed" chains of table applications.
//      if (!A.apply().hit) {
//        if (!B.apply().hit) {
//          C.apply();
//        }
//      }
//
// Below is an example control block.
// A.apply();
// B.apply();
// if (!C.apply().hit) {
//   D.Apply();
// }
// E.apply();
// if (!F.apply().hit) {
//   if (!G.apply().hit) {
//     H.apply();
//   }
// }
// I.apply();
//
// This control block generates the following pipeline:
// A --> B --> C --> E --> F --> I
//             |           |
//             D           G
//                         |
//                         H
//
// This maps to a physical pipeline with the following physical tables ordered
// by descending priority:
// Physical table 1 implements Logical tables (I)
// Physical table 2 implements Logical tables (F > G > H)
// Physical table 3 implements Logical tables (E)
// Physical table 4 implements Logical tables (C > D)
// Physical table 5 implements Logical tables (B)
// Physical table 6 implements Logical tables (A)
class BcmAclPipeline {
 public:
  // A physical table is represented as a vector of logical tables.
  using PhysicalTableAsVector = std::vector<BcmAclPipelineTable>;

  ~BcmAclPipeline() {}

  // Creates an ACL pipeline from a P4ControlBlock. Returns an error if the
  // block cannot be converted to an ACL pipeline object. The control block is
  // expected to be a top-level control block containing instructions for the
  // entire ACL bank.
  static ::util::StatusOr<std::unique_ptr<BcmAclPipeline>> CreateBcmAclPipeline(
      const P4ControlBlock& control_block);

  // Returns a constant reference to the built pipeline.
  const std::vector<PhysicalTableAsVector>& pipeline() const {
    return logical_pipeline_;
  }

  // Return a string for the pipeline. The string represents tables by:
  //   TableName
  //   (Priority)
  // Physical tables are ordered chronologically from left-to-right. Logical
  // tables that are a part of the same physical table are listed vertically
  // within the same column.
  //
  // Example:
  // table1 --> table2 --> table4 --> table7
  // (   2)     (   4)     (   7)     (   8)
  //     |          |          |
  // table8     table3     table5
  // (   1)     (   3)     (   6)
  //                           |
  //                       table6
  //                       (   5)
  std::string LogicalPipelineAsString();

 private:
  struct P4ControlTableRefHash {
    uint32 operator()(const P4ControlTableRef& x) const { return x.table_id(); }
  };

  struct P4ControlTableRefEq {
    bool operator()(const P4ControlTableRef& x,
                    const P4ControlTableRef& y) const {
      return x.table_id() == y.table_id();
    }
  };

  BcmAclPipeline() {}

  // Processes a P4ControlBlock with a root condition and updates the logical
  // pipeline. The root condition may be empty.
  ::util::Status ProcessControlBlock(const P4ControlBlock& block,
                                     const P4BranchCondition& condition);

  // Appends a logical table to the logical pipeline. If the pipeline stage is
  // occupied, the table is added to the end of the PhysicalTableAsVector. If
  // the pipeline_stage == logical_pipeline_.size(), a new PhysicalTableAsVector
  // containing the table is appended to the logical pipeline.
  //
  // Returns an error if the stage is invalid or if the table is already in the
  // pipeline.
  ::util::Status ApplyTable(const P4ControlTableRef& table, int pipeline_stage);

  // Appends a logical table to a physical table based on an on-miss root table.
  // If no other table already depends on the on-miss table, the apply-table is
  // appended to the end of the PhysicalTableAsVector containing the on-miss
  // table.
  //
  // Returns an error if another table already depends on the on-miss table or
  // if the on-hit table has not been processed.
  ::util::Status ApplyTableOnMiss(const P4ControlTableRef& apply_table,
                                  const P4ControlTableRef& on_miss);

  // Processes a control branch (if condition).
  ::util::Status ProcessBranch(const P4IfStatement& branch);

  // Processes an 'if (!table.apply().hit)' control branch.
  ::util::Status ProcessHitBranch(const P4IfStatement& branch);

  // Assigns priorities to BcmAclPipelineTables. This should be used after the
  // logical pipeline is built.
  void AssignPriorities();

  // The logical pipeline. Each entry in the vector can be seen as a new
  // physical table or as a new pipeline stage.
  std::vector<PhysicalTableAsVector> logical_pipeline_;
  // Map of table references to the pipeline stage. This is equivalent to a map
  // from table references to PhysicalTableAsVectors.
  gtl::flat_hash_map<P4ControlTableRef, int, P4ControlTableRefHash,
                     P4ControlTableRefEq>
      pipeline_stages_;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_ACL_PIPELINE_H_
