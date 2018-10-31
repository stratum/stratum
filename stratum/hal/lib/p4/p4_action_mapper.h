// The P4ActionMapper class acts as a helper for P4TableMapper.  Given a
// P4Runtime action ID, P4ActionMapper determines whether the p4c compiler
// has created an internal action to replace the native P4 program action.
// The compiler typically generates internal actions when it combines multiple
// P4 logical tables into one physical table for the target platform.  The
// internal action consolidates P4 action functions from all logical tables
// into one combined action for the physical table.

#ifndef STRATUM_HAL_LIB_P4_P4_ACTION_MAPPER_H_
#define STRATUM_HAL_LIB_P4_P4_ACTION_MAPPER_H_

#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "absl/container/flat_hash_map.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {

// The lifetime of a P4ActionMapper spans the P4PipelineConfig's life.  The
// P4TableMapper constructs a new P4ActionMapper for each new P4PipelineConfig
// push, replacing the P4ActionMapper for the previous P4PipelineConfig.  The
// normal usage sequence is:
//  - Construct a P4ActionMapper for the pushed P4PipelineConfig.
//  - Call AddP4Actions to create mapping entries for each action in the new
//    pipeline config.
//  - Call MapActionID or MapActionIDAndTableID to choose the action descriptor
//    for any P4 action ID in a P4RuntimeRequest.
//  - Delete the P4ActionMapper after a subsequent P4PipelineConfig push.
// A P4ActionMapper has no explicit lock protection.  It becomes immutable
// after AddP4Actions returns, so it is safe for all threads to read following
// initialization.
class P4ActionMapper {
 public:
  explicit P4ActionMapper(const P4PipelineConfig& p4_pipeline_config);
  virtual ~P4ActionMapper();

  // TODO(teverman): This class needs a Verify method to check
  // p4_pipeline_config vs. P4Info during config push.

  // AddP4Actions initializes P4TableMapper's internal map from the repeated
  // actions field in the pushed P4Info.  It returns a non-OK status if
  // any of the input actions do not have the necessary P4PipelineConfig data.
  // Under ordinary circumstances, errors should never occur because
  // P4ConfigVerifier should confirm the presence of all necessary internal
  // action references.
  ::util::Status AddP4Actions(const P4InfoManager& p4_info_manager);

  // MapActionIDAndTableID and MapActionID attempt to find the appropiate
  // action descriptor for the input object ID(s) from a P4Runtime request.
  // The returned descriptor refers to either the original action from the
  // P4 program or an internal action synthesized by the p4c compiler.  Since
  // the compiler can restrict internal actions to certain P4 table IDs, the
  // best practice is to call MapActionIDAndTableID for all P4Runtime requests
  // that provide both P4 IDs.  MapActionID works for P4Runtime requests
  // that do not identify P4 tables, such as action profile groups and members.
  // Since it has no table ID, MapActionID always returns an error status if
  // it finds a mapping to an internal action with table restrictions.  (Note:
  // Hercules P4 programs do not generate any such mappings.)  Both methods
  // return an error if the input action ID is invalid.
  ::util::StatusOr<const P4ActionDescriptor*> MapActionIDAndTableID(
      uint32 action_id, uint32 table_id) const;
  ::util::StatusOr<const P4ActionDescriptor*> MapActionID(
      uint32 action_id) const;

  // P4ActionMapper is neither copyable nor movable.
  P4ActionMapper(const P4ActionMapper&) = delete;
  P4ActionMapper& operator=(const P4ActionMapper&) = delete;

 private:
  // An ActionMapEntry contains the data to map from a P4 action ID to an
  // action descriptor:
  //  original_action - points to the action descriptor for the original action
  //      compiled from the P4 program.  This member is always non-NULL.
  //  internal_action - points to the action descriptor for a p4c-generated
  //      internal action.  For many actions, the internal_action descriptor
  //      always replaces the original action.  In a few limited actions,
  //      the replacement occurs only for a limited set of tables, as defined
  //      by the qualified_tables_map.  The internal_action can be NULL if
  //      original_action never redirects to an internal_action.
  //  qualified_tables_map - limits the substitution of internal_action for
  //      original_action to the table IDs in the map.  If the map is empty,
  //      internal_action replaces all P4Runtime uses of original_action.
  //      The map is also empty if original_action never redirects to an
  //      internal_action.
  // The pointers in this struct refer to descriptors in the injected
  // P4PipelineConfig; hence they are not owned by this class.
  struct ActionMapEntry {
    explicit ActionMapEntry(const P4ActionDescriptor* action)
        : original_action(action),
          internal_action(nullptr) {}

    const P4ActionDescriptor* original_action;
    const P4ActionDescriptor* internal_action;
    absl::flat_hash_map<uint32, const P4ActionDescriptor*> qualified_tables_map;
  };

  // AddAction handles the simple case where the internal_action has no
  // applied_tables qualifiers.  AddAppliedTableAction handles the more
  // complex case where internal_action is only used by the applied_tables
  // in internal_link.  Both methods update map_entry with data about when
  // internal_action replaces the original_action.
  ::util::Status AddAction(
      const P4ActionDescriptor& internal_action, ActionMapEntry* map_entry);
  ::util::Status AddAppliedTableAction(
      const P4InfoManager& p4_info_manager,
      const P4ActionDescriptor::P4InternalActionLink& internal_link,
      const P4ActionDescriptor& internal_action, ActionMapEntry* map_entry);

  // The p4_pipeline_config_ is injected, and not owned by this class.
  const P4PipelineConfig& p4_pipeline_config_;

  // P4ActionMapper uses action_map_ to find the ActionMapEntry for
  // a given action ID.
  absl::flat_hash_map<uint32, ActionMapEntry*> action_map_;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_ACTION_MAPPER_H_
