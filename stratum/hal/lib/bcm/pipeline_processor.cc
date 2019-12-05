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

#include "stratum/hal/lib/bcm/pipeline_processor.h"

#include <algorithm>

#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_join.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/lib/macros.h"
#include "stratum/public/lib/error.h"
#include "stratum/public/proto/p4_annotation.pb.h"

namespace stratum {

namespace hal {
namespace bcm {

namespace {
// Verify and collapse a valid condition map. Removes any redundant conditions
// and verifies that there are no conflicting conditions in the map.
// If the map contains conflicting conditions, returns an error and does not
// modify the valid condition map.
util::Status CollapseValidConditionMap(ValidConditionMap* conditions) {
  // Check UDP/UDP_PAYLOAD consistency.
  const auto udp_payload_iter = conditions->find(P4_HEADER_UDP_PAYLOAD);
  if (udp_payload_iter != conditions->end()) {
    const auto udp_iter = conditions->find(P4_HEADER_UDP);
    if (udp_iter != conditions->end()) {
      if (udp_payload_iter->second != udp_iter->second) {
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Inconsistent UDP Header/UDP Payload expectations.";
      }
    }
  }

  // L3 and L4 headers are mutually exclusive within the same layer. First,
  // validate the exclusivity and find the applicable headers.
  P4HeaderType l3_header = P4_HEADER_UNKNOWN;
  P4HeaderType l4_header = P4_HEADER_UNKNOWN;
  absl::flat_hash_set<P4HeaderType> l3_headers = {P4_HEADER_ARP, P4_HEADER_IPV4,
                                                  P4_HEADER_IPV6};
  absl::flat_hash_set<P4HeaderType> l4_headers = {P4_HEADER_GRE, P4_HEADER_ICMP,
                                                  P4_HEADER_TCP, P4_HEADER_UDP,
                                                  P4_HEADER_UDP_PAYLOAD};
  for (const auto& condition : *conditions) {
    P4HeaderType header = condition.first;
    bool valid = condition.second;
    if (valid) {
      if (l3_headers.count(header)) {
        if (l3_header != P4_HEADER_UNKNOWN) {
          return MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Conflicting L3 headers (" << P4HeaderType_Name(l3_header)
                 << ", " << P4HeaderType_Name(header)
                 << ") cannot be valid at once.";
        }
        l3_header = header;
      } else if (l4_headers.count(header)) {
        if (l4_header != P4_HEADER_UNKNOWN) {
          // Skip UDP/UDP_Payload conflicts.
          if ((l4_header == P4_HEADER_UDP && header == P4_HEADER_UDP_PAYLOAD) ||
              (l4_header == P4_HEADER_UDP_PAYLOAD && header == P4_HEADER_UDP)) {
            continue;
          }
          return MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Conflicting L4 headers (" << P4HeaderType_Name(l4_header)
                 << ", " << P4HeaderType_Name(header)
                 << ") cannot be valid at once.";
        }
        l4_header = header;
      }
    }
  }

  // Erase the extra conditions.
  if (l3_header != P4_HEADER_UNKNOWN) {
    l3_headers.erase(l3_header);
    for (const auto& header : l3_headers) {
      conditions->erase(header);
    }
  }
  if (l4_header != P4_HEADER_UNKNOWN) {
    l4_headers.erase(l4_header);
    for (const auto& header : l4_headers) {
      conditions->erase(header);
    }
  }

  return util::OkStatus();
}
}  // namespace

std::string PipelineProcessor::PipelineNodeAsString(const PipelineNode& node) {
  // Process Valid conditions.
  std::vector<std::string> valid_strings;
  for (const auto& valid_condition : node.valid_conditions) {
    valid_strings.push_back(
        absl::StrCat("    ", P4HeaderType_Name(valid_condition.first), ":",
                     (valid_condition.second ? "true" : "false")));
  }
  std::string valid_conditions = absl::StrJoin(valid_strings, ", ");

  // Process subtables.
  std::vector<std::string> subtable_strings;
  for (const auto& subtable : node.subtables) {
    subtable_strings.push_back(subtable->table.table_name());
  }
  std::string subtables = absl::StrJoin(subtable_strings, ", ");

  return absl::StrCat("Table: (", node.table.table_name(), ") ",
                      "Conditions: (", valid_conditions, ") ", "Parent: (",
                      node.parent.ShortDebugString(), ") ", "Priority: (",
                      node.priority, ") ", "Subtables: (", subtables, ")");
}

std::string PipelineProcessor::PhysicalPipelineAsString() {
  if (physical_pipeline_.empty()) return "";

  // Generate template strings for each stage. Template types:
  // For strings (table names): "        "
  // For integers (priorities): "(      )"
  //              For dividers: "      | "
  std::vector<std::string> stage_str_templates;
  std::vector<std::string> stage_int_templates;
  std::vector<std::string> stage_div_templates;
  for (const PhysicalTableAsVector& stage : physical_pipeline_) {
    // Find max stage (column) width.
    size_t length = 0;
    for (const PipelineTable& table : stage) {
      length = std::max(length, table.table.table_name().size());
      length = std::max(length, absl::StrCat("(", table.priority, ")").size());
    }
    // Create the stage template strings.
    std::string padding;
    padding.insert(0, length - 2, ' ');
    stage_str_templates.push_back(absl::StrCat(padding, "  "));
    stage_int_templates.push_back(absl::StrCat("(", padding, ")"));
    stage_div_templates.push_back(absl::StrCat(padding, "| "));
  }

  std::vector<std::string> output_lines;
  // Tables are added by physical table depth. Each depth contains 2-3 lines:
  //   Divider row (not used at depth 0).
  //   Table name row.
  //   Table ID row.
  // Tables that do not have entries are left blank.
  // Example output:
  // table1 --> table2 --> table4 --> table7 | Name Row     (Depth 0)
  // (   2)     (   4)     (   7)     (   8) | Priority Row (Depth 0)
  //     |          |          |             | Divider Row  (Depth 1)
  // table8     table3     table5            | Name Row     (Depth 1)
  // (   1)     (   3)     (   6)            | Priority Row (Depth 1)
  //                           |             | Divider Row  (Depth 2)
  //                       table6            | Name Row     (Depth 2)
  //                       (   5)            | Priority Row (Depth 2)
  size_t depth = 0;
  bool tables_found;
  do {
    std::vector<std::string> table_divs;
    std::vector<std::string> names;
    std::vector<std::string> priorities;
    tables_found = false;

    for (size_t stage = 0; stage < physical_pipeline_.size(); ++stage) {
      const PhysicalTableAsVector& stage_tables = physical_pipeline_[stage];
      if (stage_tables.size() > depth) {
        // Add dividers, names, and priorities for present tables.
        table_divs.push_back(stage_div_templates.at(stage));
        // Add the right-justified table name.
        std::string formatted_name = stage_str_templates.at(stage);
        const auto& table_name = stage_tables.at(depth).table.table_name();
        formatted_name.replace(formatted_name.size() - table_name.size(),
                               table_name.size(), table_name);
        names.push_back(formatted_name);
        // Add the right-justified-within-parentheses priority.
        std::string formatted_priority = stage_int_templates.at(stage);
        std::string priority = absl::StrCat(stage_tables.at(depth).priority);
        formatted_priority.replace(
            formatted_priority.size() - priority.size() - 1, priority.size(),
            priority);
        priorities.push_back(formatted_priority);
        tables_found = true;
      } else {
        // Add spacing for missing tables.
        names.push_back(stage_str_templates.at(stage));
        priorities.push_back(stage_str_templates.at(stage));
        table_divs.push_back(stage_str_templates.at(stage));
      }
    }
    // Skip output step if there were no tables.
    if (!tables_found) continue;

    // Add the divider, name, and id lines to the output vector.
    if (depth == 0) {
      output_lines.push_back(absl::StrJoin(names, " --> "));
    } else {
      output_lines.push_back(absl::StrJoin(table_divs, "     "));
      output_lines.push_back(absl::StrJoin(names, "     "));
    }
    output_lines.push_back(absl::StrJoin(priorities, "     "));
    ++depth;
  } while (tables_found);

  // Remove trailing spaces from everything.
  for (auto& line : output_lines) {
    size_t end = line.find_last_not_of(' ');
    if (end != std::string::npos) {
      line.erase(end + 1);
    }
  }

  return absl::StrJoin(output_lines, "\n");
}

::util::StatusOr<std::unique_ptr<PipelineProcessor>>
PipelineProcessor::CreateInstance(const P4ControlBlock& control_block) {
  std::unique_ptr<PipelineProcessor> pipeline_processor =
      absl::WrapUnique<PipelineProcessor>(new PipelineProcessor());
  RETURN_IF_ERROR(pipeline_processor->ProcessControlBlock(
      control_block, pipeline_processor->root_));
  RETURN_IF_ERROR(pipeline_processor->SetPriorities());
  pipeline_processor->PopulatePhysicalPipeline();
  return std::move(pipeline_processor);
}

::util::Status PipelineProcessor::ProcessControlBlock(
    const P4ControlBlock& block, const PipelineNode& base_node) {
  VLOG(1) << "Node: \n" << PipelineNodeAsString(base_node);
  VLOG(1) << "ControlBlock: \n" << block.DebugString();
  for (const P4ControlStatement& statement : block.statements()) {
    std::string error = absl::StrCat(" Failed to process statement (",
                                     statement.ShortDebugString(), ").");
    switch (statement.statement_case()) {
      case P4ControlStatement::kApply:
        RETURN_IF_ERROR_WITH_APPEND(ApplyTable(statement.apply(), base_node))
            << error;
        break;
      case P4ControlStatement::kBranch:
        switch (statement.branch().condition().condition_case()) {
          case P4BranchCondition::kHit:
            RETURN_IF_ERROR_WITH_APPEND(
                ProcessHitBranch(statement.branch(), base_node))
                << error;
            break;
          case P4BranchCondition::kIsValid:
            RETURN_IF_ERROR_WITH_APPEND(
                ProcessIsValidBranch(statement.branch(), true, base_node))
                << error;
            RETURN_IF_ERROR_WITH_APPEND(
                ProcessIsValidBranch(statement.branch(), false, base_node))
                << error;
            break;
          default:
            LOG(WARNING) << "Ignoring unknown branch statement "
                         << statement.branch().ShortDebugString();
        }
        break;
      case P4ControlStatement::kDrop:
      case P4ControlStatement::kReturn:
      case P4ControlStatement::kExit:
      case P4ControlStatement::kFixedPipeline:
        // TODO(richardyu): Handle these if we extend the scope past ACLs.
        break;
      case P4ControlStatement::kOther:
        LOG(WARNING) << "Ignoring unknown control statement "
                     << statement.other();
        break;
      case P4ControlStatement::STATEMENT_NOT_SET:
        // Empty case; don't do anything.
        break;
    }
  }
  return ::util::OkStatus();
}

::util::Status PipelineProcessor::ApplyTable(const P4ControlTableRef& table,
                                             PipelineNode node) {
  // Find the position for this node.
  PipelineNode* parent;
  if (!node.has_parent()) {
    parent = &root_;
  } else {
    parent = gtl::FindOrNull(table_to_node_, node.parent);
  }
  if (parent == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Failed to lookup parent table ("
                                    << node.parent.ShortDebugString()
                                    << ") while applying table. This is a bug.";
  }
  if (node.has_parent() &&
      node.parent.pipeline_stage() != table.pipeline_stage()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Pipeline stage mismatch for parent table ("
           << parent->table.table_id() << ").";
  }
  // Any conditions that apply to the parent table also apply to this table.
  for (const auto& condition : parent->valid_conditions) {
    ::util::Status condition_status =
        InsertIfNotConflicting(&node.valid_conditions, condition);
    if (!condition_status.ok()) {
      LOG(WARNING) << "Conflicting header valid conditions found between a "
                      "table and its parent: "
                   << condition_status.error_message() << " Skipping table.";
      return ::util::OkStatus();
    }
  }
  // Create the node and add it to the graph.
  if (table_to_node_.count(table)) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Cannot apply a table more than once.";
  }
  table_to_node_.emplace(table, std::move(node));
  table_to_node_[table].table = table;
  table_to_node_[table].subtables.clear();
  parent->subtables.push_back(&table_to_node_[table]);

  return ::util::OkStatus();
}

::util::Status PipelineProcessor::ProcessHitBranch(
    const P4IfStatement& branch, const PipelineNode& base_node) {
  const P4ControlTableRef& hit_table = branch.condition().hit();
  const PipelineNode* hit_node = gtl::FindOrNull(table_to_node_, hit_table);
  if (hit_node == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Cannot branch on a table before it is applied.";
  }
  if (base_node.has_parent() &&
      !P4ControlTableRefEq()(base_node.parent, hit_node->parent)) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Inconsistent dependency relationships between table apply and "
              "on-miss conditional.";
  }
  if ((branch.has_false_block() && branch.condition().not_operator()) ||
      (branch.has_true_block() && !branch.condition().not_operator())) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "On-hit actions are not supported by Stratum.";
  }
  PipelineNode branch_node = base_node;
  branch_node.parent = hit_table;
  const P4ControlBlock& statement = branch.condition().not_operator()
                                        ? branch.true_block()
                                        : branch.false_block();
  return ProcessControlBlock(statement, branch_node);
}

::util::Status PipelineProcessor::ProcessIsValidBranch(
    const P4IfStatement& branch, bool is_valid, PipelineNode node) {
  // Find the correct control block for the is_valid setting.
  const P4ControlBlock* block = &branch.true_block();
  if (is_valid == branch.condition().not_operator()) {
    block = &branch.false_block();
  }
  if (block->statements().empty()) {
    return ::util::OkStatus();
  }

  auto valid_condition =
      std::make_pair(branch.condition().is_valid().header_type(), is_valid);

  // Skip this branch if we don't have the header type.
  if (valid_condition.first == P4HeaderType::P4_HEADER_UNKNOWN) {
    LOG(WARNING) << "Skipping unknown is_valid condition: "
                 << branch.condition().is_valid().DebugString() << ".";
    return ::util::OkStatus();
  }

  // Skip this branch if we have a conflicting condition. This branch will never
  // be true.
  ::util::Status condition_status =
      InsertIfNotConflicting(&node.valid_conditions, valid_condition);
  if (!condition_status.ok()) {
    LOG(WARNING) << "Skipping conflicting condition for "
                 << branch.condition().is_valid().header_name() << " is "
                 << (is_valid ? "valid" : "not valid") << ": "
                 << condition_status.error_message()
                 << " Statement: " << block->ShortDebugString() << ".";
    return ::util::OkStatus();
  }
  return ProcessControlBlock(*block, node);
}

::util::Status PipelineProcessor::SetPriorities() {
  // Process the priorities from right-to-left (latest-to-earliest).
  int priority = table_to_node_.size();
  for (auto subtable = root_.subtables.rbegin();
       subtable != root_.subtables.rend(); ++subtable) {
    RETURN_IF_ERROR(SetPriority(&priority, *subtable));
  }
  return ::util::OkStatus();
}

::util::Status PipelineProcessor::SetPriority(int* priority,
                                              PipelineNode* node) {
  if (*priority <= 0) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "There are more tables in the pipeline graph than tables that "
              "have been allocated. This is a bug.";
  }
  node->priority = *priority;
  --(*priority);
  for (auto subtable = node->subtables.rbegin();
       subtable != node->subtables.rend(); ++subtable) {
    RETURN_IF_ERROR(SetPriority(priority, *subtable));
  }
  return ::util::OkStatus();
}

void PipelineProcessor::PopulatePhysicalPipeline() {
  for (const auto& table : root_.subtables) {
    physical_pipeline_.emplace_back();
    AppendToPhysicalTable(*table, &physical_pipeline_.back());
  }
}

void PipelineProcessor::AppendToPhysicalTable(
    const PipelineNode& node, PhysicalTableAsVector* physical_table) {
  physical_table->push_back(NodeToTable(node));
  for (auto table = node.subtables.rbegin(); table != node.subtables.rend();
       ++table) {
    AppendToPhysicalTable(**table, physical_table);
  }
}

PipelineTable PipelineProcessor::NodeToTable(const PipelineNode& node) {
  PipelineTable table;
  table.table = node.table;
  table.valid_conditions = node.valid_conditions;
  table.priority = node.priority;
  return table;
}

::util::Status PipelineProcessor::InsertIfNotConflicting(
    ValidConditionMap* destination, std::pair<P4HeaderType, bool> condition) {
  auto lookup = destination->find(condition.first);
  if (lookup != destination->end()) {
    if (lookup->second != condition.second) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Conflicting value for header type "
             << P4HeaderType_Name(condition.first) << " already exists.";
    }
  }
  auto insert_iter = destination->insert(std::move(condition)).first;
  util::Status status = CollapseValidConditionMap(destination);
  if (!status.ok()) {
    destination->erase(insert_iter);
  }
  return status;
}

}  // namespace bcm
}  // namespace hal

}  // namespace stratum
