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

#ifndef STRATUM_HAL_LIB_BCM_PIPELINE_PROCESSOR_H_
#define STRATUM_HAL_LIB_BCM_PIPELINE_PROCESSOR_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/p4/common_flow_entry.pb.h"
#include "stratum/hal/lib/p4/p4_control.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/p4_table_defs.pb.h"

namespace stratum {

namespace hal {
namespace bcm {

// Stores a mapping of header types to their is-valid conditions.
// True: the header must be valid.
// False: The header must be invalid.
using ValidConditionMap =
    absl::flat_hash_map<P4HeaderType, bool, EnumHash<P4HeaderType>>;

// Contains ACL table information extracted from the p4 control flow.
struct PipelineTable {
  PipelineTable() : table(), valid_conditions(), priority(0) {}
  P4ControlTableRef table;
  ValidConditionMap valid_conditions;
  int priority;
  // TODO(richardyu): Add applied actions to this structure.
};

// PipelineProcessor parses a P4ControlBlock and returns the tables it
// processes. The tables are represented references to the P4 tables with
// attached information garnered from the pipeline, including required
// conditions for applying the table and priority. The resulting pipeline groups
// tables into physical tables based on their dependencies. If a table only
// applies when another table misses, that table must be a lower-priority table
// within the same physical table. Currently, P4ControlBlock is only expected to
// correctly generate ACL tables.
class PipelineProcessor {
 public:
  // A physical table is represented as a vector of logical tables.
  using PhysicalTableAsVector = std::vector<PipelineTable>;

  // Destructor.
  virtual ~PipelineProcessor() {}

  // Creates an ACL pipeline from a P4ControlBlock. Returns an error if the
  // block cannot be converted to an ACL pipeline object. The control block is
  // expected to be a top-level control block containing instructions for the
  // entire ACL bank.
  static ::util::StatusOr<std::unique_ptr<PipelineProcessor>> CreateInstance(
      const P4ControlBlock& control_block);

  // Returns the pipeline as a vector of physical tables. Each physical table is
  // a vector of PipelineTable objects, each representing a logical table.
  const std::vector<PhysicalTableAsVector>& PhysicalPipeline() const {
    return physical_pipeline_;
  }

  // Returns the physical pipeline (from PhysicalPipeline()) in a string format.
  std::string PhysicalPipelineAsString();

 protected:
  // The constructor is hidden. This class should be built using the public
  // factory function.
  PipelineProcessor() : root_(), table_to_node_() {}

 private:
  // Functions used for mainting hashes of P4ControlTableRef based on table id.
  struct P4ControlTableRefHash {
    uint32 operator()(const P4ControlTableRef& x) const { return x.table_id(); }
  };
  struct P4ControlTableRefEq {
    bool operator()(const P4ControlTableRef& x,
                    const P4ControlTableRef& y) const {
      return x.table_id() == y.table_id();
    }
  };

  // Node information used when traversing the pipeline. Each node represents
  // one potential table.
  struct PipelineNode {
    PipelineNode()
        : table(), valid_conditions(), parent(), priority(0), subtables() {}
    PipelineNode(const PipelineNode& other)
        : table(other.table),
          valid_conditions(other.valid_conditions),
          parent(other.parent),
          priority(other.priority),
          subtables(other.subtables) {}
    bool has_parent() const { return parent.table_id() != 0; }
    P4ControlTableRef table;
    ValidConditionMap valid_conditions;
    P4ControlTableRef parent;
    int priority;
    std::vector<PipelineNode*> subtables;
  };

  // Returns a string describing a PipelineNode.
  std::string PipelineNodeAsString(const PipelineNode& node);

  // Processes a P4ControlBlock with a base node.
  ::util::Status ProcessControlBlock(const P4ControlBlock& block,
                                     const PipelineNode& base_node);

  // Processes a table Apply P4 control statement. Adds the table to
  // table_to_node_ and to the pipeline graph.
  ::util::Status ApplyTable(const P4ControlTableRef& table, PipelineNode node);

  // Processes a Hit branch statement. Updates the parent for a node.
  ::util::Status ProcessHitBranch(const P4IfStatement& branch,
                                  const PipelineNode& base_node);

  // Processes an Isvalid branch statement. Updates the valid conditions for a
  // node.
  ::util::Status ProcessIsValidBranch(const P4IfStatement& branch,
                                      bool is_valid, PipelineNode node);

  // Sets the priorities for all the tables in the pipeline graph. This should
  // be called after the graph is otherwise complete.
  // Priorities use the following rules:
  // * In any node, the earliest subtable has the lowest priority. Priority
  //   increases up to the latest subtable, which has the highest priority.
  // * In any node, all priorities of a  direct subtable and all of its children
  //   are either lower or highter than any other direct subtable of the node.
  // * A node has a higher priority than any of its subtables (except root,
  //   which has no priority).
  ::util::Status SetPriorities();

  // Recursively sets the priority for a node and all of its subtables.
  ::util::Status SetPriority(int* priority, PipelineNode* node);

  // Populates physical_pipeline_ using the pipeline graph. This should be
  // called after the graph is complete and SetProrities() has been called.
  void PopulatePhysicalPipeline();

  // Appends a node and all of its children to the physical table.
  void AppendToPhysicalTable(const PipelineNode& node,
                             PhysicalTableAsVector* physical_table);

  // Creates and returns a PipelineTable object based on a PipelineNode object.
  PipelineTable NodeToTable(const PipelineNode& node);

  // Inserts a valid condition into a valid condition map if there is not
  // already a conflicting rule in the map. Two rules conflict if the header
  // type is the same but the condition is opposite (e.g. IPv4 is valid & IPv4
  // is invalid).
  static ::util::Status InsertIfNotConflicting(
      ValidConditionMap* destination, std::pair<P4HeaderType, bool> condition);

  // The root node for the pipeline graph. This node only has subtables.
  PipelineNode root_;

  // Mapping from table references to pipeline nodes for tables that have been
  // applied in the pipeline. We use a node_hash_map, which has pointer
  // stability, since we keep pointers to the PipelineNodes.
  absl::node_hash_map<P4ControlTableRef, PipelineNode, P4ControlTableRefHash,
                      P4ControlTableRefEq>
      table_to_node_;

  // The physical tables represented as a pipeline. Each PhysicalTableAsVector
  // object is a new physical table. Each PhysicalTableAsVector contains all
  // logical tables within the physical table.
  std::vector<PhysicalTableAsVector> physical_pipeline_;
};

}  // namespace bcm
}  // namespace hal

}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_PIPELINE_PROCESSOR_H_
