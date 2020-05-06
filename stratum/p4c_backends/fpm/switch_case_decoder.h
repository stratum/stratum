// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// The SwitchCaseDecoder is a p4c Inspector subclass that visits the node
// hierarchy under an IR::SwitchStatement in a P4 control function.  It looks
// for supported actions within the statement cases and translates then into
// additional table map output for affected action descriptors.

#ifndef STRATUM_P4C_BACKENDS_FPM_SWITCH_CASE_DECODER_H_
#define STRATUM_P4C_BACKENDS_FPM_SWITCH_CASE_DECODER_H_

#include <map>
#include <string>
#include <vector>
#include <utility>

#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"

namespace stratum {
namespace p4c_backends {

// A single SwitchCaseDecoder handles all IR::SwitchStatements within a P4
// program.  In normal usage, the backend constructs an instance, then invokes
// the Decode method for each SwitchStatement it encounters during P4Control
// processing.  The SwitchCaseDecoder assumes that the backend has already
// processed the P4 program's actions, and the TableMapGenerator contains
// action descriptors for any action labels it finds in SwitchStatement cases.
class SwitchCaseDecoder : public Inspector {
 public:
  // The action_name_map facilitates translation from the internal action names
  // in SwitchStatement nodes to the external action names in the P4 table map
  // output.  The ref_map and type_map parameters are part of the p4c midend
  // output.  The table_mapper updates action descriptor data with output for
  // supported switch statements.
  SwitchCaseDecoder(const std::map<std::string, std::string>& action_name_map,
                    P4::ReferenceMap* ref_map, P4::TypeMap* type_map,
                    TableMapGenerator* table_mapper);
  ~SwitchCaseDecoder() override {}

  // The Decode method should be called once for each SwitchStatement in a
  // P4Control.  It verifies that the SwitchStatement operations are valid and
  // supported by Stratum.  It calls table_mapper to update the action
  // descriptors with operations for the switch to perform.  It reports P4
  // program errors through p4c's ErrorReporter.  Decode expects that a
  // MeterColorMapper has previously transformed metering conditions into
  // IR::MeterColorStatements.
  virtual void Decode(const IR::SwitchStatement& switch_statement);

  // Following a normal Decode, the caller can find the table applied by
  // the SwitchStatement expression via this accessor.  The accessor returns
  // nullptr before Decode is called or if SwitchStatement contains an
  // unexpected expression type.
  virtual const IR::P4Table* applied_table() const { return applied_table_; }

  // These methods override the IR::Inspector base class to visit the nodes
  // under the inspected IR::SwitchStatement.  Per p4c convention, the preorder
  // functions return true to visit deeper nodes in the IR, or false if the
  // SwitchCaseDecoder does not need to visit any deeper nodes.
  bool preorder(const IR::BlockStatement* statement) override;
  bool preorder(const IR::MeterColorStatement* statement) override;
  bool preorder(const IR::Statement* statement) override;

  // SwitchCaseDecoder is neither copyable nor movable.
  SwitchCaseDecoder(const SwitchCaseDecoder&) = delete;
  SwitchCaseDecoder& operator=(const SwitchCaseDecoder&) = delete;

 protected:
  // Simpler constructor - suitable for mock subclass use only.
  explicit SwitchCaseDecoder(
      const std::map<std::string, std::string>& action_name_map);

 private:
  // Reinitializes all members related to the state of the most recent Decode
  // run, including the individual case state members.
  void ClearDecodeState();

  // Reinitializes all members related to the state of the current switch case.
  void ClearCaseState();

  // These members record the constructor parameters.
  const std::map<std::string, std::string>& action_name_map_;
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;
  TableMapGenerator* table_mapper_;

  // Refers to the table that was applied by the SwitchStatement expression.
  const IR::P4Table* applied_table_;

  // This member contains pending table_mapper_ updates, which are stored
  // here until all switch cases are successfully decoded without p4c errors.
  // The first member of the pair is the action name, and the second member
  // is the data for table_mapper_ to append to the action descriptor.
  std::vector<std::pair<std::string, std::string>> color_actions_;

  // These members track the decoded state of the current case.
  std::string action_;     // Name of the action affected by the case.
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_SWITCH_CASE_DECODER_H_
