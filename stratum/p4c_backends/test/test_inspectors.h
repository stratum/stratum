/*
 * Copyright 2019 Google LLC
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

// This file contains IR Inspector subclasses to collect data for unit tests.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_TEST_TEST_INSPECTORS_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_TEST_TEST_INSPECTORS_H_

#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "external/com_github_p4lang_p4c/ir/visitor.h"

namespace stratum {
namespace p4c_backends {

// The two IR Inspector subclasses below examine P4Control nodes for
// information that tests use to verify optimization passes.  Each class
// expects to operate on one IR::P4Control instance.  A StatementCounter
// visits selected IR nodes to count how many times certain interesting types
// of statements occur.
class StatementCounter : public Inspector {
 public:
  StatementCounter()
      : pipeline_statement_count_(0),
        if_statement_count_(0),
        block_statement_count_(0),
        hit_statement_count_(0) {
  }
  ~StatementCounter() override {}

  // Visits nodes under p4_control and accumulates counts for interesting
  // statement types, which tests can subsequently access via the accessors.
  void CountStatements(const IR::P4Control& p4_control) {
    p4_control.body->apply(*this);
  }

  // These preorder methods accumulate counts for their respective statement
  // types.
  bool preorder(const IR::IfStatement* statement) override {
    ++if_statement_count_;
    return true;  // Enables visits of deeper nodes.
  }
  bool preorder(const IR::PipelineStageStatement* statement) override {
    ++pipeline_statement_count_;
    // Any statements under the PipelineStageStatement have been optimized
    // away from the Stratum perspective, so the return value avoids visits
    // to deeper nodes.
    return false;
  }
  bool preorder(const IR::BlockStatement* statement) override {
    ++block_statement_count_;
    return true;  // Enables visits of deeper nodes.
  }
  bool preorder(const IR::TableHitStatement* statement) override {
    ++hit_statement_count_;
    return true;  // Enables visits of deeper nodes (in case TableHitStatement
                  // ever has any child nodes).
  }

  // Tests can query the statement counters with these accessors.
  int pipeline_statement_count() const { return pipeline_statement_count_; }
  int if_statement_count() const { return if_statement_count_; }
  int block_statement_count() const { return block_statement_count_; }
  int hit_statement_count() const { return hit_statement_count_; }

  // StatementCounter is neither copyable nor movable.
  StatementCounter(const StatementCounter&) = delete;
  StatementCounter& operator=(const StatementCounter&) = delete;

 private:
  // These members accumulate counts of statement types.
  int pipeline_statement_count_;
  int if_statement_count_;
  int block_statement_count_;
  int hit_statement_count_;
};

// An OptimizedTableInspector visits IR nodes to find tables that have been
// "optimized" into a PipelineStageStatement.  It inserts each table name
// into one of two sets, depending on whether the table apply occurs within
// the scope of a PipelineStageStatement.
class OptimizedTableInspector : public Inspector {
 public:
  OptimizedTableInspector() : pipeline_depth_(0) {}
  ~OptimizedTableInspector() override {}

  // Visits nodes under p4_control and sorts them into "optimized" and
  // "unoptimized" sets.  After InspectTables returns, tests can query the
  // optimized status of each table with the IsOptimized/IsUnoptimized methods.
  // It is technically feasible for one table to be in both sets if it appears
  // in two different branches of p4_control, and only one of them has been
  // optimized.
  void InspectTables(const IR::P4Control& p4_control) {
    p4_control.body->apply(*this);
  }

  // The pre/postorder methods for PipelineStageStatements update the depth
  // of nested statements.
  bool preorder(const IR::PipelineStageStatement* statement) override {
    ++pipeline_depth_;
    return true;  // Enables visits to deeper IR nodes.
  }
  void postorder(const IR::PipelineStageStatement* statement) override {
    --pipeline_depth_;
  }

  // The PathExpression preorder method looks for expressions that refer to
  // tables.  When it finds a table, it adds the table's name to one of
  // the two sets, depending on whether pipeline_depth_ indicates that this
  // table reference is "optimized" into a PipelineStageStatement.
  bool preorder(const IR::PathExpression* path_expression) override {
    if (path_expression->type->is<IR::Type_Table>()) {
      auto ir_table = path_expression->type->to<IR::Type_Table>()->table;
      if (pipeline_depth_)
        optimized_tables_.insert(std::string(ir_table->externalName()));
      else
        unoptimized_tables_.insert(std::string(ir_table->externalName()));
    }
    return true;  // Enables visits to deeper IR nodes.
  }

  // Behaves like the PathExpression preorder above, but gets the table
  // reference from a TableHitStatement.
  bool preorder(const IR::TableHitStatement* statement) override {
    if (pipeline_depth_)
      optimized_tables_.insert(std::string(statement->table_name));
    else
      unoptimized_tables_.insert(std::string(statement->table_name));
    return false;  // Disables visits to deeper IR nodes.
  }

  // These two methods report the optimized status of a table; results are
  // valid only after return from InspectTables.
  bool IsOptimized(const std::string& table_name) const {
    return optimized_tables_.find(table_name) != optimized_tables_.end();
  }
  bool IsUnoptimized(const std::string& table_name) const {
    return unoptimized_tables_.find(table_name) != unoptimized_tables_.end();
  }

  // OptimizedTableInspector is neither copyable nor movable.
  OptimizedTableInspector(const OptimizedTableInspector&) = delete;
  OptimizedTableInspector& operator=(const OptimizedTableInspector&) = delete;

 private:
  int pipeline_depth_;  // Tracks nesting level of PipelineStageStatements.

  // These two sets contain the names of tables according to their
  // optimization status.
  std::set<std::string> optimized_tables_;
  std::set<std::string> unoptimized_tables_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_TEST_TEST_INSPECTORS_H_
