// This library contains class that help build p4 protobufs for testing.

#ifndef PLATFORMS_NETWORKING_HERCULES_LIB_TEST_UTILS_P4_PROTO_BUILDERS_H_
#define PLATFORMS_NETWORKING_HERCULES_LIB_TEST_UTILS_P4_PROTO_BUILDERS_H_

#include "stratum/hal/lib/p4/p4_control.pb.h"
#include "stratum/public/proto/p4_annotation.pb.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "absl/strings/str_cat.h"
#include "p4/config/v1/p4info.pb.h"

namespace google {
namespace hercules {
namespace test_utils {
namespace p4_proto_builders {

// Support class to build a P4ControlTableRef message from inputs or an existing
// table's preamble. The P4ControlTableRef is used to generate Apply statements
// and Hit conditions in a P4ControlBlock.
class P4ControlTableRefBuilder {
 public:
  // Construct an empty table.
  P4ControlTableRefBuilder() : reference_() {}
  // Construct a table reference with the ID & name of a table.
  explicit P4ControlTableRefBuilder(const ::p4::config::v1::Preamble& preamble)
      : reference_() {
    reference_.set_table_id(preamble.id());
    reference_.set_table_name(preamble.name());
  }

  // Construct a table reference with the ID & name of a table.
  explicit P4ControlTableRefBuilder(const ::p4::config::v1::Table& table)
      : P4ControlTableRefBuilder(table.preamble()) {}

  // Construct a table reference with the ID & name of a table and a provided
  // stage.
  explicit P4ControlTableRefBuilder(const ::p4::config::v1::Preamble& preamble,
                                    P4Annotation::PipelineStage stage)
      : P4ControlTableRefBuilder(preamble) {
    reference_.set_pipeline_stage(stage);
  }

  // Construct a table reference with the ID & name of a table and a provided
  // stage
  explicit P4ControlTableRefBuilder(const ::p4::config::v1::Table& table,
                                    P4Annotation::PipelineStage stage)
      : P4ControlTableRefBuilder(table.preamble(), stage) {}

  // Set the ID of this table reference.
  P4ControlTableRefBuilder& Id(int table_id) {
    reference_.set_table_id(table_id);
    return *this;
  }
  // Set the name of this table reference.
  P4ControlTableRefBuilder& Name(const string& table_name) {
    reference_.set_table_name(table_name);
    return *this;
  }
  // Set the stage of this table reference.
  P4ControlTableRefBuilder& Stage(P4Annotation::PipelineStage table_stage) {
    reference_.set_pipeline_stage(table_stage);
    return *this;
  }
  // Return this table reference.
  const hal::P4ControlTableRef& Build() const { return reference_; }

 private:
  hal::P4ControlTableRef reference_;
};

// Support class to build a P4ControlStatement rooted at an on-table-hit
// condition. The P4ControlStatement will have a P4IfStatement with a Hit
// condition.
class HitBuilder {
 public:
  HitBuilder() : on_hit_(false), use_false_(false) {}

  // Builds and returns the P4ControlStatement based on the state of this
  // builder.
  hal::P4ControlStatement Build();

  // Creates an on-table-miss condition for the control block.
  HitBuilder& OnMiss(hal::P4ControlTableRef table) {
    condition_ = std::move(table);
    on_hit_ = false;
    return *this;
  }

  // Creates an on-table-hit condition for the control block.
  HitBuilder& OnHit(hal::P4ControlTableRef table) {
    condition_ = std::move(table);
    on_hit_ = true;
    return *this;
  }

  // Put the condition in the false block instead of the true block. This
  // inverts the condition (e.g. OnHit will produce !hit.false).
  HitBuilder& UseFalse() {
    use_false_ = true;
    return *this;
  }

  // Sets the control block to execute when the condition is met.
  HitBuilder& ControlBlock(hal::P4ControlBlock block) {
    control_block_ = std::move(block);
    return *this;
  }

  // Adds an action to the execution control block.
  HitBuilder& Do(hal::P4ControlStatement statement) {
    *control_block_.add_statements() = std::move(statement);
    return *this;
  }

  // Appends statements in a control block to the execution control block.
  HitBuilder& Do(const hal::P4ControlBlock& block) {
    control_block_.mutable_statements()->MergeFrom(block.statements());
    return *this;
  }

 private:
  bool on_hit_;     // Whether the action should be conditional on hit or miss.
  bool use_false_;  // Whether to invert the conditions with the not_operator.
  hal::P4ControlTableRef condition_;   // The table for the hit condition.
  hal::P4ControlBlock control_block_;  // The control block to apply.
};

// Support class for build a P4ControlStatement with a root is_valid
// condition.
class IsValidBuilder {
 public:
  IsValidBuilder()
      : use_not_(false),
        header_type_(P4HeaderType::P4_HEADER_UNKNOWN),
        valid_control_block_(),
        invalid_control_block_() {}

  // Builds and returns the P4ControlStatement based on the state of this
  // builder.
  hal::P4ControlStatement Build();

  // Sets the header type for the is_valid condition.
  IsValidBuilder& Header(P4HeaderType header_type) {
    header_type_ = header_type;
    return *this;
  }

  // Sets the control block to apply when the condition is valid.
  IsValidBuilder& ValidControlBlock(hal::P4ControlBlock block) {
    valid_control_block_ = std::move(block);
    return *this;
  }

  // Sets the control block to apply when the condition is not valid.
  IsValidBuilder& InvalidControlBlock(hal::P4ControlBlock block) {
    invalid_control_block_ = std::move(block);
    return *this;
  }

  // Append an action to the valid control block.
  IsValidBuilder& DoIfValid(hal::P4ControlStatement statement) {
    *valid_control_block_.add_statements() = std::move(statement);
    return *this;
  }

  // Appends actions to apply when the condition is valid.
  IsValidBuilder& DoIfValid(const hal::P4ControlBlock& block) {
    valid_control_block_.mutable_statements()->MergeFrom(block.statements());
    return *this;
  }

  // Append an action to the invalid control block.
  IsValidBuilder& DoIfInvalid(hal::P4ControlStatement statement) {
    *invalid_control_block_.add_statements() = std::move(statement);
    return *this;
  }

  // Appends actions to apply when the condition is not valid.
  IsValidBuilder& DoIfInvalid(const hal::P4ControlBlock& block) {
    invalid_control_block_.mutable_statements()->MergeFrom(block.statements());
    return *this;
  }

  // Puts the valid condition in the false block and the invalid condition in
  // the true block. Sets condition.not_operator to true.
  IsValidBuilder& UseNot() {
    use_not_ = true;
    return *this;
  }

 private:
  bool use_not_;  // set condition.not_operator() and swap true/false blocks.
  P4HeaderType header_type_;  // Header type used in the is_valid check.
  hal::P4ControlBlock valid_control_block_;    // Block applied when valid.
  hal::P4ControlBlock invalid_control_block_;  // Block applied when invalid.
};

// Build and return a default table based on an ID and optional stage.
inline hal::P4ControlTableRef Table(
    int id, P4Annotation::PipelineStage stage = P4Annotation::INGRESS_ACL) {
  P4ControlTableRefBuilder builder;
  return builder.Id(id).Name(absl::StrCat("table_", id)).Stage(stage).Build();
}

// Build and return a P4ControlStatement that applies a default table based on
// an ID and optional stage.
inline hal::P4ControlStatement ApplyTable(
    int id, P4Annotation::PipelineStage stage = P4Annotation::INGRESS_ACL) {
  hal::P4ControlStatement statement;
  *statement.mutable_apply() = Table(id, stage);
  return statement;
}

// Build and return a P4ControlStatement that applies a default table based on
// an ID and optional stage.
inline hal::P4ControlStatement ApplyTable(
    const ::p4::config::v1::Table& table,
    P4Annotation::PipelineStage stage = P4Annotation::INGRESS_ACL) {
  hal::P4ControlStatement statement;
  *statement.mutable_apply() = P4ControlTableRefBuilder(table, stage).Build();
  return statement;
}

// Build and return a P4ControlStatement that applies a default table based on
// an ID and optional stage.
inline hal::P4ControlStatement ApplyTable(
    const ::p4::config::v1::Preamble& preamble,
    P4Annotation::PipelineStage stage = P4Annotation::INGRESS_ACL) {
  hal::P4ControlStatement statement;
  *statement.mutable_apply() =
      P4ControlTableRefBuilder(preamble, stage).Build();
  return statement;
}

// This function returns a P4ControlBlock that applies a set of tables nested
// under each other in order. 3-table example:
// apply tables[0]
// if not hit tables[0]:
//   apply tables[1]
//   if not hit tables[1]:
//     apply tables[2]
hal::P4ControlBlock ApplyNested(std::vector<hal::P4ControlTableRef> tables);
hal::P4ControlBlock ApplyNested(std::vector<::p4::config::v1::Table> tables,
                                P4Annotation::PipelineStage stage);

}  // namespace p4_proto_builders
}  // namespace test_utils
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_LIB_TEST_UTILS_P4_PROTO_BUILDERS_H_
