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


// P4ConfigVerifier verifies consistency among various P4 objects in the P4Info
// and the P4PipelineConfig.  It helps P4TableMapper verify forwarding
// pipeline config pushes.  It also has a role in some unit tests that verify
// Hercules p4c output.  It may also be directly integrated into p4c to detect
// invalid output.

#ifndef STRATUM_HAL_LIB_P4_P4_CONFIG_VERIFIER_H_
#define STRATUM_HAL_LIB_P4_P4_CONFIG_VERIFIER_H_

#include <memory>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "p4/config/v1/p4info.pb.h"

namespace stratum {
namespace hal {

// An instance of P4ConfigVerifier operates on a single P4Info and
// P4PipelineConfig message pair.  Normal usage is to create the instance
// with the message pair to verify, then call one of the Verify methods to
// evaluate consistency among objects across both messages.  P4ConfigVerifier
// assumes that a P4InfoManager has already checked the P4Info internal
// consistency.
class P4ConfigVerifier {
 public:
  // The creation parameters provide the P4Info and P4PipelineConfig
  // to verify.  The caller must assure that both messages remain in scope
  // throughout the life of the P4ConfigVerifier instance.
  static std::unique_ptr<P4ConfigVerifier> CreateInstance(
      const ::p4::config::v1::P4Info& p4_info,
      const P4PipelineConfig& p4_pipeline_config);
  virtual ~P4ConfigVerifier() {}

  // Verify iterates over P4 objects in p4_info_ and p4_pipeline_config_,
  // making sure the messages are consistent.  For example, all p4_info_
  // tables need to have a matching table descriptor in p4_pipeline_config_,
  // and all p4_info_ match keys need a p4_pipeline_config_ field descriptor.
  // The return status is OK when verification succeeds.  If one or more
  // failures occur, Verify returns ERR_INTERNAL.  Verify attempts to find
  // as many inconsistencies as possible, so the returned status may report
  // multiple errors.
  virtual ::util::Status Verify();

  // VerifyAndCompare performs a superset of the Verify method.  In addition
  // to the basic verification of P4 objects, it compares the injected P4Info
  // and P4PipelineConfig to parameters referencing previous versions.  This
  // comparison evaluates whether the differences between the old and new
  // versions can be achieved without a switch reboot.  The return status is
  // OK when the basic Verify succeeds and no reboot is required.  If the
  // basic Verify fails, VerifyAndCompare returns its status.  If the basic
  // Verify succeeds, but a reboot is needed to achieve either the P4Info
  // or P4PipelineConfig changes, the return status is ERR_REBOOT_REQUIRED.
  // VerifyAndCompare assumes that the old versions of both inputs have
  // previously passed verification.
  virtual ::util::Status VerifyAndCompare(
      const ::p4::config::v1::P4Info& old_p4_info,
      const P4PipelineConfig& old_p4_pipeline_config);

 private:
  // The constructor is private; use public CreateInstance method.
  P4ConfigVerifier(const ::p4::config::v1::P4Info& p4_info,
                   const P4PipelineConfig& p4_pipeline_config)
      : p4_info_(p4_info), p4_pipeline_config_(p4_pipeline_config) {}

  // Verifies the input p4_table, which comes from one of the P4Info table
  // entries.
  ::util::Status VerifyTable(const ::p4::config::v1::Table& p4_table);

  // Verifies the input p4_action, which comes from one of the P4Info action
  // entries.
  ::util::Status VerifyAction(const ::p4::config::v1::Action& p4_action);

  // Verifies the input static_entry, which comes from one of the static table
  // entries in the P4PipelineConfig.
  ::util::Status VerifyStaticTableEntry(const ::p4::v1::Update& static_entry);

  // Verifies the input match_field, which is part of the P4Info for table_name.
  ::util::Status VerifyMatchField(
      const ::p4::config::v1::MatchField& match_field,
      const std::string& table_name);

  // Verifies the contents of the given action_descriptor.
  ::util::Status VerifyActionDescriptor(
      const P4ActionDescriptor& action_descriptor,
      const std::string& action_name, bool check_action_redirects);

  // Verifies the contents of the given action_descriptor with specific
  // constraints for internal actions.
  ::util::Status VerifyInternalAction(
      const P4ActionDescriptor& action_descriptor,
      const std::string& action_name);

  // Verifies the input action instructions, which are part of the action
  // descriptor for action_name.
  ::util::Status VerifyActionInstructions(
      const P4ActionDescriptor::P4ActionInstructions& instructions,
      const std::string& action_name);

  // Verifies any links to internal actions within the input action_descriptor.
  ::util::Status VerifyInternalActionLinks(
      const P4ActionDescriptor& action_descriptor,
      const std::string& action_name);

  // These two methods verify assignments within P4 action bodies.
  ::util::Status VerifyHeaderAssignment();
  ::util::Status VerifyFieldAssignment(
      const std::string& destination_field,
      const P4AssignSourceValue& source_value, const std::string& action_name);

  // Verifies that the input P4FieldDescriptor contains a known field type.
  bool VerifyKnownFieldType(const P4FieldDescriptor& descriptor);

  // Attempts to find the field descriptor for field_name in the
  // p4_pipeline_config_ table map.  The log_object is the name of the P4
  // object that refers to the field, typically a table or an action name.
  ::util::StatusOr<const P4FieldDescriptor*> GetFieldDescriptor(
      const std::string& field_name, const std::string& log_object);

  // Attempts to find the action descriptor for internal_action_name in the
  // p4_pipeline_config_ table map.  The log_object is the name of the P4
  // object that refers to the action, typically an action name.
  ::util::StatusOr<const P4ActionDescriptor*> GetInternalActionDescriptor(
      const std::string& internal_action_name, const std::string& log_object);

  // Filters errors according to levels specified by command-line flags.
  static ::util::Status FilterError(const std::string& message,
                                    const std::string& filter_level);

  // P4 configuration messages to verify.
  const ::p4::config::v1::P4Info& p4_info_;
  const P4PipelineConfig& p4_pipeline_config_;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_CONFIG_VERIFIER_H_
