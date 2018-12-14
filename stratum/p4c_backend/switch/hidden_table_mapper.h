// The HiddenTableMapper class handles tables in the P4 program that are
// marked by the @switchstack "HIDDEN" pipeline stage annotation.  Hercules
// treats some of these tables as logical extensions of some other P4 table in
// a physical pipeline stage.  The tables for packet encap/decap are one
// example.  The P4 programs split encap (and also decap) across two tables.
// The first table makes the encap (or decap) decision and records it in a
// local metadata field.  The second table, applied somewhere later in the
// pipeline, performs the actual encap (or decap) operations on the applicable
// packet headers.  On Hercules targets, this table pair maps to a single
// physical table, and the p4c backend populates the P4PipelineConfig with
// data that allows the switch stack to merge actions from both tables into
// the relevant physical table.  Hercules characterizes these tables as follows:
//
//  1) The P4 table must be hidden.
//  2) The P4 table must have a single local metadata field as a match key.
//  3) The match key field in (2) is only assigned constant values.  NetInfra
//     has agreed that assigning an action parameter to these match keys
//     adds too much complexity.  (Example, what if the action parameter value
//     in the P4 runtime request does not match any static entry in the
//     hidden table?)
//  4) The action assignments for the field in (2) occur in a physical table.
//  5) The table in item (2) has only static entries.
//
// Subsequent comments refer to local metadata fields matching these
// circumstances as "indirect action keys".

#ifndef PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_HIDDEN_TABLE_MAPPER_H_
#define PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_HIDDEN_TABLE_MAPPER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "platforms/networking/hercules/hal/lib/p4/p4_info_manager.h"
#include "platforms/networking/hercules/hal/lib/p4/p4_pipeline_config.host.pb.h"
#include "platforms/networking/hercules/hal/lib/p4/p4_table_map.host.pb.h"
#include "platforms/networking/hercules/public/proto/p4_table_defs.host.pb.h"

namespace google {
namespace hercules {
namespace p4c_backend {

// A HiddenTableMapper instance searches the P4PipelineConfig and P4Info
// for tables with "indirect action keys", as described by the file header
// comments.  It updates the P4PipelineConfig field descriptors for qualified
// keys with additional information linking the key assignments to the
// selection of actions in subsequent tables.  The HiddenTableMapper also
// produces a map of action descriptors that operate on the "indirect action
// keys".  These descriptors contain pending changes to the P4PipelineConfig
// based on the HiddenTableMapper's knowledge, but they cannot be fully
// updated until a subsequent static entry mapping pass occurs. The normal
// HiddenTableMapper usage is to create an instance, run ProcessTables with
// the P4Info and P4PipelineConfig that the backend has generated thus far,
// and then pass the pending descriptor outputs along to a static table entry
// remapping step.
class HiddenTableMapper {
 public:
  // This map contains action descriptors that may need modifications to
  // incorporate references to hidden tables.  The key is the action name.
  // The values are action descriptors with pending updates for hidden table
  // support, but the descriptors cannot be fully modified and validated
  // until they can be correlated with static table entries after the
  // HiddenTableMapper completes its work.
  typedef std::map<std::string, hal::P4ActionDescriptor> ActionRedirectMap;

  HiddenTableMapper() {}
  virtual ~HiddenTableMapper();

  // ProcessTables does the work to find indirect action keys and
  // update the corresponding field descriptors.  The P4InfoManager contains
  // the P4Info output from p4c.  The p4_pipeline_cfg is an input/output
  // parameter.  Upon return, HiddenTableMapper replaces field descriptors for
  // applicable indirect action key fields, and it also produces a separate
  // map of pending action descriptor updates, which is available via the
  // action_redirects() accessor.
  void ProcessTables(const hal::P4InfoManager& p4_info_manager,
                     hal::P4PipelineConfig* p4_pipeline_cfg);

  // Accessors.
  const ActionRedirectMap& action_redirects() const {
    return action_redirects_;
  }

  // HiddenTableMapper is neither copyable nor movable.
  HiddenTableMapper(const HiddenTableMapper&) = delete;
  HiddenTableMapper& operator=(const HiddenTableMapper&) = delete;

 private:
  // IndirectActionKey is a private helper class for HiddenTableMapper.  Each
  // instance of IndirectActionKey represents a potential qualifying indirect
  // action key usage.
  class IndirectActionKey {
   public:
    // The constructor's field_name identifies the potential qualifying key,
    // such as "local_metadata.decap_type".  The ActionRedirectMap provides a
    // place for this instance to store any pending updates to action
    // descriptors.  It is owned by the HiddenTableMapper that creates the
    // IndirectActionKey, and it may contain action descriptor updates by
    // other IndirectActionKey instances.
    IndirectActionKey(const std::string& field_name,
                      ActionRedirectMap* action_redirects)
        : field_name_(field_name),
          action_redirects_(action_redirects),
          disqualified_(false) {}
    virtual ~IndirectActionKey() {}

    // QualifyKey examines P4Info and P4PipelineConfig data to determine
    // whether this IndirectActionKey instance meets the qualifications for a
    // local metadata field whose only usage is as a key in deferred table
    // lookups.  It returns false if this instance can never be used as an
    // IndirectActionKey, ragardless of the input p4_table.  A true return
    // means one of two things:
    //  1) This instance qualifies as an IndirectActionKey for the input
    //     p4_table, but it is not guaranteed to be valid for use as a
    //     key for other hidden tables.
    //  2) This instance does not qualify for the input p4_table, but it
    //     is not necessarily disqualified from being used by other tables.
    bool QualifyKey(const ::p4::config::v1::MatchField& match_field,
                    const ::p4::config::v1::Table& p4_table,
                    const hal::P4PipelineConfig& p4_pipeline_cfg);

    // FindActions looks for all the actions in the P4 table map that assign
    // a value to this instance's field_name_.  It records any qualifying
    // action and the assigned value for future use.
    void FindActions(const hal::P4PipelineConfig& p4_pipeline_cfg);

    // When two IndirectActionKeys are present for the same field's use
    // across multiple tables, this method combines them into one instance.
    void Merge(const IndirectActionKey& source_key);

    // Accessors.
    const std::string& field_name() const { return field_name_; }
    const hal::P4FieldDescriptor& new_field_descriptor() const {
      return new_field_descriptor_;
    }
    const std::set<std::string>& qualified_tables() const {
      return qualified_tables_;
    }
    bool disqualified() const { return disqualified_; }

    // IndirectActionKey is neither copyable nor movable.
    IndirectActionKey(const IndirectActionKey&) = delete;
    IndirectActionKey& operator=(const IndirectActionKey&) = delete;

   private:
    // This method processes an assignment to this key's field_name_ in
    // old_action_descriptor.  The source_value is a value that the action
    // assigns to the key.
    void HandleKeyAssignment(
        const std::string& action_name,
        const hal::P4ActionDescriptor& old_action_descriptor,
        const P4AssignSourceValue& source_value);

    // Searches the assignments in the input action descriptor and outputs
    // a vector of matching index values from descriptor.assignments().
    // Example: If the input descriptor has 5 assignments and the 3rd one
    // assigns to field_name_, the output vector contains {2}.
    void FindAssignmentsToKey(const std::string& action_name,
                              const hal::P4ActionDescriptor& descriptor,
                              std::vector<int>* assignment_indexes);

    // Upon successful processing of this IndirectActionKey instance, this
    // method removes all action assignments to field_name_.  The assignments
    // are superseded by action_redirect entries in the action descriptor.
    void RemoveAssignmentsToKey();

    // The field_name_ member identifies the match field represented by this
    // IndirectActionKey instance.
    const std::string field_name_;

    // This member is injected by the constructor.
    ActionRedirectMap *action_redirects_;

    // This P4FieldDescriptor stores table map updates relative to a local
    // metadata field's role as an indirect table lookup key.  If this
    // instance meets all the necessary qualifications, this descriptor
    // eventually replaces the P4PipelineConfig field descriptor.
    hal::P4FieldDescriptor new_field_descriptor_;

    // Maintains the set of hidden tables that are qualified to use this
    // IndirectActionKey instance.
    std::set<std::string> qualified_tables_;

    // This member becomes true if IndirectActionKey detects any conditions
    // that prevent this instance from being used as a hidden table key.
    bool disqualified_;

    // This map records all the actions that set field_name_, primarily for
    // detecting duplicate assignments to the same field.  The key is the
    // action name.  The value contains the constant that the action assigns
    // to field_name_ for hidden table lookup.  The value is meaningless
    // when disqualified_ is true.
    std::map<std::string, int64> actions_assigns_;
  };

  // Evaluates the input p4_table to determine whether its key consists
  // of a single local metadata match field that meets other qualifying
  // conditions to be an IndirectActionKey.
  void CheckTableForIndirectActionKey(
      const ::p4::config::v1::Table& p4_table,
      const hal::P4PipelineConfig& p4_pipeline_cfg);

  // When match_field qualifies as an IndirectActionKey, this method creates
  // or updates an entry in meta_key_map_.
  void CreateOrUpdateQualifiedKey(
      const ::p4::config::v1::MatchField& match_field,
      const ::p4::config::v1::Table& p4_table,
      const hal::P4PipelineConfig& p4_pipeline_cfg);

  // This map contains all fields currently under consideration for use as
  // an IndirectActionKey.
  std::map<std::string, IndirectActionKey*> meta_key_map_;

  // This map contains any action descriptors that need updates to reflect
  // assignments to an IndirectActionKey.
  ActionRedirectMap action_redirects_;
};

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_HIDDEN_TABLE_MAPPER_H_
