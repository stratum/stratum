// This file implements the Hercules p4c backend's ControlInspector.

#include "platforms/networking/hercules/p4c_backend/switch/control_inspector.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "platforms/networking/hercules/p4c_backend/switch/condition_inspector.h"
#include "platforms/networking/hercules/p4c_backend/switch/internal_action.h"
#include "platforms/networking/hercules/p4c_backend/switch/p4_model_names.host.pb.h"
#include "platforms/networking/hercules/p4c_backend/switch/utils.h"
#include "absl/debugging/leak_check.h"
#include "p4lang_p4c/frontends/p4/methodInstance.h"
#include "p4lang_p4c/frontends/p4/tableApply.h"
#include "sandblaze/p4lang/p4/config/v1/p4info.host.pb.h"
#include "util/gtl/map_util.h"

namespace google {
namespace hercules {
namespace p4c_backend {

ControlInspector::ControlInspector(const hal::P4InfoManager* p4_info_manager,
                                   P4::ReferenceMap* ref_map,
                                   P4::TypeMap* type_map,
                                   SwitchCaseDecoder* switch_case_decoder,
                                   TableMapGenerator* table_map_generator)
    : p4_info_manager_(ABSL_DIE_IF_NULL(p4_info_manager)),
      ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)),
      switch_case_decoder_(ABSL_DIE_IF_NULL(switch_case_decoder)),
      table_map_generator_(ABSL_DIE_IF_NULL(table_map_generator)),
      working_block_(nullptr),
      working_statement_(nullptr) {}

void ControlInspector::Inspect(const IR::P4Control& control) {
  if (!control_.name().empty()) {
    LOG(ERROR) << "ControlInspector does not inspect multiple IR::P4Controls";
    return;
  }

  control_.set_name(control.externalName());
  if (control_.name() == GetP4ModelNames().ingress_control_name())
    control_.set_type(hal::P4Control::P4_CONTROL_INGRESS);
  else if (control_.name() == GetP4ModelNames().egress_control_name())
    control_.set_type(hal::P4Control::P4_CONTROL_EGRESS);
  else
    control_.set_type(hal::P4Control::P4_CONTROL_UNKNOWN);
  working_block_ = control_.mutable_main();
  nested_blocks_.push_back(working_block_);

  // This inspector is applied to visit the IR nodes in the input control
  // via the preorder methods.
  {
    absl::LeakCheckDisabler disable_ir_control_leak_checks;
    control.body->apply(*this);
  }
  VLOG(1) << "Inspected control " << control_.DebugString();
  AppendMeterActions();
}

bool ControlInspector::preorder(const IR::IfStatement* if_statement) {
  condition_.Clear();
  auto table_hit = P4::TableApplySolver::isHit(
      if_statement->condition, ref_map_, type_map_);
  DCHECK(table_hit == nullptr)
      << "Unexpected table.apply().hit in IR::IfStatement condition. "
      << "Check for incompatible frontend or midend transformations.";
  bool condition_hidden = !DecodeCondition(*if_statement->condition);

  // Even if the condition is hidden, the true and false blocks still need
  // to be processed for meters, clones, and drops that need to migrate
  // to actions.
  AddStatement();
  *working_statement_->mutable_branch()->mutable_condition() = condition_;
  auto working_if_statement = working_statement_;
  StartTrueBlock();
  visit(if_statement->ifTrue);
  if (if_statement->ifFalse) {
    StartFalseBlock();
    visit(if_statement->ifFalse);
  }
  EndBlock();
  table_hit_ = "";

  // The true_block and false_block can both be empty.  This occurs if
  // all statements have been moved from the control to actions, which
  // can happen for metering decisions.  In this case, the last statement
  // is erased from the working block, so it is important to do this after
  // the EndBlock treatment pops to the expected block level.  Hidden
  // conditions also need to be erased here.
  if (condition_hidden ||
      (working_if_statement->branch().true_block().statements_size() == 0 &&
       working_if_statement->branch().false_block().statements_size() == 0)) {
    EraseLastStatementInBlock();
  }

  return true;
}

// The ControlInspector may visit MethodCallExpressions representing these
// use cases in the P4Control:
//  - An apply method on a table.  This case generally originates from
//    visiting the right-hand side of an assignment statement that assigns
//    the table apply result to a temporary hit variable.
//  - Calling an extern method.
//  - Calling one of the built-in methods, including isValid on a header.
//  - A P4 action call.
bool ControlInspector::preorder(const IR::MethodCallExpression* mce) {
  P4::MethodInstance* instance =
      P4::MethodInstance::resolve(mce, ref_map_, type_map_);
  if (instance->isApply()) {
    DecodeApplyTable(*instance->to<P4::ApplyMethod>());
  } else if (instance->is<P4::ExternMethod>()) {
    AddStatement();
    // TODO(teverman): Evaluate for additional support in Hercules use cases.
    const std::string pseudo_code = StringPrintf(
        "extern method %s", instance->to<P4::ExternMethod>()->
            originalExternType->name.toString().c_str());
    working_statement_->set_other(pseudo_code);
  } else if (instance->is<P4::BuiltInMethod>()) {
    auto built_in = instance->to<P4::BuiltInMethod>();
    if (built_in->name == IR::Type_Header::isValid) {
      AddValidHeaderCondition(built_in->appliedTo->toString().c_str());
    } else {
      AddStatement();
      // TODO(teverman): Evaluate for additional support in Hercules use cases.
      const std::string pseudo_code = StringPrintf(
          "built-in method %s",
          instance->to<P4::BuiltInMethod>()->name.toString().c_str());
      working_statement_->set_other(pseudo_code);
    }
  } else if (instance->is<P4::ActionCall>()) {
    AddStatement();
    // TODO(teverman): Evaluate for additional support in Hercules use cases.
    working_statement_->set_other("MethodCallExpression action call");
  } else if (instance->is<P4::ExternFunction>()) {
    AddStatement();
    auto extern_func = instance->to<P4::ExternFunction>();
    if (extern_func->method->name.toString() ==
        GetP4ModelNames().drop_extern_name()) {
      working_statement_->set_drop(true);
    } else {
      const std::string pseudo_code = StringPrintf(
          "MethodCallExpression extern function %s",
          extern_func->method->name.toString().c_str());
      working_statement_->set_other(pseudo_code);
    }
  } else {
    AddStatement();
    // TODO(teverman): Evaluate for additional support in Hercules use cases.
    working_statement_->set_other("Unknown MethodCallExpression");
  }
  return true;
}

// The ControlInspector expects to encounter assignment statements in these
// use cases:
//  - Control assignments to metadata and header fields.
//  - Compiler-generated assignment of a table hit result to a temporary
//    variable, now DEPRECATED by IR::TableHitStatement.
bool ControlInspector::preorder(const IR::AssignmentStatement* assignment) {
  auto table_hit =
      P4::TableApplySolver::isHit(assignment->right, ref_map_, type_map_);
  DCHECK(table_hit == nullptr) << "Did HitAssignMapper transform run?";
  AddStatement();
  // TODO(teverman): There are two items to consider here:
  //  1) Support for various field assignments by the control.
  //  2) Can the existing code for assignment statements in action bodies
  //     be refactored to support this?
  working_statement_->set_other("Assignment statement");
  return true;
}

bool ControlInspector::preorder(const IR::TableHitStatement* hit_statement) {
  AddApplyStatement(*hit_statement->p4_table);
  AddHitVar(hit_statement->hit_var_name.c_str(), hit_statement->p4_table);
  return false;  // No need to visit deeper nodes.
}

bool ControlInspector::preorder(const IR::ExitStatement* exit_statement) {
  AddStatement();
  working_statement_->set_exit(true);
  return false;  // No need to visit deeper nodes.
}

bool ControlInspector::preorder(const IR::ReturnStatement* return_statement) {
  AddStatement();
  working_statement_->set_return_(true);
  return false;  // No need to visit deeper nodes.
}

// PipelineStageStatement nodes represent a statement or block of statements
// that earlier optimization passes have placed into a fixed pipeline stage.
// ControlInspector encodes this statement using a PipelineInspector to identify
// all the tables that belong to the statement's pipeline stage.
bool ControlInspector::preorder(
    const IR::PipelineStageStatement* pipeline_statement) {
  AddStatement();
  auto fixed_pipeline = working_statement_->mutable_fixed_pipeline();
  fixed_pipeline->set_pipeline_stage(
      static_cast<P4Annotation::PipelineStage>(pipeline_statement->stage));
  PipelineInspector pipeline_inspector(*p4_info_manager_, fixed_pipeline);
  pipeline_inspector.GetTableList(*pipeline_statement);
  return false;  // The deeper nodes have been optimized out.
}

// ControlInspector hands off all the work for a SwitchStatement to the
// injected SwitchCaseDecoder.  It is still ControlInspector's responsibility
// to emit the apply statement for the table in the SwitchStatement expression.
bool ControlInspector::preorder(const IR::SwitchStatement* switch_statement) {
  switch_case_decoder_->Decode(*switch_statement);
  if (switch_case_decoder_->applied_table())
    AddApplyStatement(*switch_case_decoder_->applied_table());
  return false;  // No need to visit deeper nodes.
}

// Previous inspectors have verified that the MeterColorStatement
// is valid in the current context.
bool ControlInspector::preorder(
    const IR::MeterColorStatement* meter_statement) {
  DCHECK(!table_hit_.empty())
      << "Expected MeterColorStatement to follow table hit.  Check for proper "
      << "execution of previous transforms and inspectors";
  metered_tables_.push_back(std::make_pair(table_hit_, meter_statement));
  return false;  // No need to visit deeper nodes.
}

bool ControlInspector::DecodeCondition(const IR::Expression& condition) {
  bool output_ok = true;
  const IR::PathExpression* path_expression = nullptr;
  if (condition.is<IR::LNot>()) {
    condition_.set_not_operator(true);
    if (condition.to<IR::LNot>()->expr->is<IR::PathExpression>()) {
      path_expression =
          condition.to<IR::LNot>()->expr->to<IR::PathExpression>();
    }
  } else if (condition.is<IR::PathExpression>()) {
    path_expression = condition.to<IR::PathExpression>();
  } else {
    visit(&condition);  // Visits deeper IR to find out more about condition.
    if (condition_.condition_case() ==
        hal::P4BranchCondition::CONDITION_NOT_SET) {
      // The general ConditionInspector takes over when none of the
      // specialized control conditions has been decoded.
      ConditionInspector condition_inspector;
      condition_inspector.Inspect(condition);
      condition_.set_unknown(condition_inspector.description());
    }
  }

  // A PathExpression in a condition should refer to a temporary hit variable.
  if (path_expression) {
    const std::string tmp_var_name = std::string(
        path_expression->path->name.toString());
    const auto& iter = hit_vars_map_.find(tmp_var_name);
    if (iter != hit_vars_map_.end()) {
      const IR::P4Table& ir_table = *iter->second;
      table_hit_ = ir_table.externalName().c_str();

      // The hit status of hidden tables is not recorded to the pipeline config
      // output.  This leaves an empty condition, which needs to be cleaned
      // up by the caller.
      if (GetAnnotatedPipelineStage(ir_table) != P4Annotation::HIDDEN) {
        auto hit_condition = condition_.mutable_hit();
        FillTableRefFromIR(ir_table, *p4_info_manager_, hit_condition);
      } else {
        output_ok = false;
      }
    } else {
      LOG(ERROR) << "Missing temporary variable " << tmp_var_name
                 << " for table apply.hit";
    }
  }

  return output_ok;
}

void ControlInspector::DecodeApplyTable(const P4::ApplyMethod& apply) {
  if (apply.isTableApply()) {
    AddApplyStatement(*apply.object->to<IR::P4Table>());
  } else {
    LOG(ERROR) << "MethodCallExpression is apply to non-table!";
  }
}

void ControlInspector::AddApplyStatement(const IR::P4Table& table) {
  if (!table.getAnnotation("hidden")) {
    AddStatement();
    hal::P4ControlTableRef* table_ref = working_statement_->mutable_apply();
    FillTableRefFromIR(table, *p4_info_manager_, table_ref);
  } else {
    LOG(ERROR) << "Unexpected apply to hidden table" << table.externalName();
  }
}

void ControlInspector::AddValidHeaderCondition(const std::string& header_name) {
  condition_.mutable_is_valid()->set_header_name(header_name);
  const hal::P4TableMapValue* table_map_header = gtl::FindOrNull(
      table_map_generator_->generated_map().table_map(), header_name);
  P4HeaderType header_type = P4_HEADER_UNKNOWN;
  if (table_map_header != nullptr) {
    if (table_map_header->has_header_descriptor())
      header_type = table_map_header->header_descriptor().type();
  }
  if (header_type == P4_HEADER_UNKNOWN) {
    LOG(WARNING) << "Unable to find header type for header "
                 << header_name << " in P4PipelineConfig";
  }
  condition_.mutable_is_valid()->set_header_type(header_type);
}

void ControlInspector::AddHitVar(const std::string& hit_var_name,
                                 const IR::P4Table* ir_table) {
  hit_vars_map_[hit_var_name] = ir_table;
}

void ControlInspector::AddStatement() {
  working_statement_ = working_block_->add_statements();
}

void ControlInspector::StartTrueBlock() {
  auto last_statement = GetLastStatementInBlock();
  working_block_ = last_statement->mutable_branch()->mutable_true_block();
  nested_blocks_.push_back(working_block_);
  working_statement_ = nullptr;
}

void ControlInspector::StartFalseBlock() {
  EndBlock();  // Pops the true_block before starting the false_block.
  auto last_statement = GetLastStatementInBlock();
  working_block_ = last_statement->mutable_branch()->mutable_false_block();
  nested_blocks_.push_back(working_block_);
  working_statement_ = nullptr;
}

void ControlInspector::EndBlock() {
  CHECK_GT(nested_blocks_.size(), 1);
  nested_blocks_.pop_back();
  working_block_ = nested_blocks_.back();
  working_statement_ = nullptr;
}

void ControlInspector::EraseLastStatementInBlock() {
  auto statement = working_block_->mutable_statements()->ReleaseLast();
  DCHECK(statement != nullptr);
  delete statement;
}

hal::P4ControlStatement* ControlInspector::GetLastStatementInBlock() {
  int last_index = working_block_->statements_size() - 1;
  CHECK_GE(last_index, 0) << "Expected at least one statement in working "
                          << "block";
  auto last_statement = working_block_->mutable_statements(last_index);
  CHECK(last_statement->has_branch())
      << "Expected last statement in block to be an if statement "
      << working_block_->ShortDebugString();
  return last_statement;
}

// Each table that was found to be metered during the IR inspection needs
// to have the color actions from MeterColorStatements merged into every
// non-default table action.
void ControlInspector::AppendMeterActions() {
  for (const auto& meter_table : metered_tables_) {
    auto table_status = p4_info_manager_->FindTableByName(meter_table.first);
    DCHECK(table_status.ok())
        << "Missing table " << meter_table.first << " in P4Info";
    const ::p4::config::v1::Table& p4_table = table_status.ValueOrDie();

    for (const auto& action_ref : p4_table.action_refs()) {
      bool skip_default_action = false;
      for (const auto& annotation : action_ref.annotations()) {
        if (annotation.find("@defaultonly") != string::npos) {
          skip_default_action = true;
          break;
        }
      }
      if (skip_default_action) continue;
      auto action_status = p4_info_manager_->FindActionByID(action_ref.id());
      DCHECK(action_status.ok())
          << "Missing action ID " << action_ref.id() << " in P4Info";
      const ::p4::config::v1::Action& p4_action = action_status.ValueOrDie();

      // For each affected table and action pair, the following changes occur:
      //  - The original action descriptor is copied.
      //  - A new InternalAction is created to merge the original action
      //    with the meter condition from the control body.
      //  - The original descriptor is replaced with a copy that contains
      //    an internal link to the new InternalAction.
      //  - The new InternalAction is added to the pipeline config.
      hal::P4ActionDescriptor original_descriptor_copy =
          FindActionDescriptorOrDie(p4_action.preamble().name(),
                                    table_map_generator_->generated_map());
      InternalAction internal_action(
          p4_action.preamble().name(), original_descriptor_copy,
          table_map_generator_->generated_map());
      internal_action.MergeMeterCondition(
          meter_table.second->meter_color_actions.c_str());
      hal::P4ActionDescriptor::P4InternalActionLink* internal_link =
          original_descriptor_copy.add_action_redirects()->add_internal_links();
      internal_link->set_internal_action_name(internal_action.internal_name());
      internal_link->add_applied_tables(meter_table.first);
      table_map_generator_->ReplaceActionDescriptor(
          p4_action.preamble().name(), original_descriptor_copy);
      internal_action.Optimize();
      internal_action.WriteToTableMapGenerator(table_map_generator_);
    }
  }
}

// The PipelineInspector class implementation starts here.
ControlInspector::PipelineInspector::PipelineInspector(
    const hal::P4InfoManager& p4_info_manager,
    hal::FixedPipelineTables* fixed_pipeline)
    : p4_info_manager_(p4_info_manager),
      fixed_pipeline_(ABSL_DIE_IF_NULL(fixed_pipeline)) {}

void ControlInspector::PipelineInspector::GetTableList(
    const IR::PipelineStageStatement& statement) {
  statement.apply(*this);
}

// When the PathExpression refers to a table, the output is appended with
// P4ControlTableRef data from the table's IR node.
bool ControlInspector::PipelineInspector::preorder(
    const IR::PathExpression* path_expression) {
  if (path_expression->type->is<IR::Type_Table>()) {
    FillTableRefFromIR(
        *(path_expression->type->to<IR::Type_Table>()->table),
        p4_info_manager_, fixed_pipeline_->add_tables());
  }
  return true;
}

bool ControlInspector::PipelineInspector::preorder(
    const IR::TableHitStatement* statement) {
  FillTableRefFromIR(
      *statement->p4_table, p4_info_manager_, fixed_pipeline_->add_tables());
  return false;
}

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
