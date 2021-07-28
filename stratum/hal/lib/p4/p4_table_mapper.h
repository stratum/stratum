// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_P4_P4_TABLE_MAPPER_H_
#define STRATUM_HAL_LIB_P4_P4_TABLE_MAPPER_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/p4/common_flow_entry.pb.h"
#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/hal/lib/p4/p4_static_entry_mapper.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/p4_table_defs.pb.h"

namespace stratum {
namespace hal {

// Packet in (out) metadata map types.
typedef absl::flat_hash_map<P4FieldType, std::pair<uint32, int>,
                            EnumHash<P4FieldType>>
    MetadataTypeToIdBitwidthMap;
typedef absl::flat_hash_map<uint32, std::pair<P4FieldType, int>>
    MetadataIdToTypeBitwidthMap;

// The P4TableMapper is responsible for mapping P4 forwarding entries (e.g
// TableEntry, ActionProfileGroup/Member, etc) to a vendor agnostic proto
// format for one single device (aka switching node).
// NOTE: This class itself is not thread-safe and the user needs to make sure
// it is used in a thread-safe way. BcmTableManager is already doing this.
class P4TableMapper {
 public:
  virtual ~P4TableMapper();

  // Pushes the parts of the given ChassisConfig proto that this class cares
  // about and mutate the internal state if needed. The given node_id is used to
  // understand which part of the ChassisConfig is intended for this class
  virtual ::util::Status PushChassisConfig(const ChassisConfig& config,
                                           uint64 node_id);

  // Verifies the parts of ChassisConfig proto that this class cares about. The
  // given node_id is used to understand which part of the ChassisConfig is
  // intended for this class
  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id);

  // Pushes the P4-based forwarding pipeline configuration of the single
  // switching node this class is mapped to. On the first push, this function
  // constructs internal maps to streamline the mapping between P4 object IDs
  // and their associated descriptor data, based on the specified P4Info proto
  // and a target-specific device config serialized as a byte stream given by
  // the ForwardingPipelineConfig proto. The function then decodes the byte
  // stream to its internal P4PipelineConfig proto (and reports error if
  // decoding is not possible).
  virtual ::util::Status PushForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config);

  // Verifies the P4-based forwarding pipeline configuration of the single
  // switching node this class is mapped to. This function makes sure that every
  // applicable P4 object has a known mapping.
  virtual ::util::Status VerifyForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config);

  // Performs coldboot shutdown. Note that there is no public Initialize().
  // Initialization is done as part of PushChassisConfig() if the class is not
  // initialized by the time we push config.
  virtual ::util::Status Shutdown();

  // Takes the input P4 table_entry and maps it to the output flow_entry.
  // The return status reports one of the following conditions:
  // OK - the mapping succeeds, and flow_entry contains a full translation
  //     of the input table_entry.
  // ERR_OPER_NOT_SUPPORTED - some parts of table_entry could not be mapped;
  //     The flow_entry contains a partial mapping, with unmapped fields,
  //     tables, and actions being marked by the UNKNOWN value.  In this
  //     case, the caller may attempt to translate unknown objects with some
  //     vendor-dependent procedure. As the implementation proceeds,
  //     ERR_OPER_NOT_SUPPORTED will occur less often.
  // ERR_INVALID_PARAM - the encoding of table_entry is invalid in some way,
  //     and flow_entry output is not provided.
  // ERR_INTERNAL - other errors making flow_entry output invalid.
  // This functions is expected to be called by MapFlowEntry() in
  // P4TableMapper.
  virtual ::util::Status MapFlowEntry(const ::p4::v1::TableEntry& table_entry,
                                      ::p4::v1::Update::Type update_type,
                                      CommonFlowEntry* flow_entry) const;

  // Takes the input P4 ActionProfileMember, validate it and maps it to the
  // output mapped_action, if applicable. The output contains the translated
  // data for the member's action field. The return status reports one of the
  // following conditions:
  // OK - the input member is valid, member action mapping succeeds, and
  //     mapped_action contains a full translation of the member's action data.
  // ERR_INVALID_P4_INFO - member could not be found in P4Info.
  // ERR_OPER_NOT_SUPPORTED - some parts of member could not be mapped.
  // ERR_INVALID_PARAM - the encoding of member is invalid in some way,
  //     and mapped_action output is not provided.
  // ERR_INTERNAL - other errors making mapped_action output invalid.
  virtual ::util::Status MapActionProfileMember(
      const ::p4::v1::ActionProfileMember& member,
      MappedAction* mapped_action) const;

  // Takes the input P4 ActionProfileGroup and validate it. The output
  // mapped_action contains only the type value P4_ACTION_TYPE_PROFILE_GROUP_ID.
  // This is because the ActionProfileGroup data should be sufficient to process
  // the profile request without any further mapping. The return status reports
  // one of the following conditions:
  // OK - the input group is valid.
  // ERR_INVALID_P4_INFO - group could not be found in P4Info.
  virtual ::util::Status MapActionProfileGroup(
      const ::p4::v1::ActionProfileGroup& group,
      MappedAction* mapped_action) const;

  // Converts a given MappedPacketMetadata to a P4 PacketMetadata to be
  // added to the packet before sending it to controller (used for packet in at
  // the server/switch side).
  virtual ::util::Status DeparsePacketInMetadata(
      const MappedPacketMetadata& mapped_packet_metadata,
      ::p4::v1::PacketMetadata* p4_packet_metadata) const;

  // Converts a P4 PacketMetadata received from an incoming packet from
  // controller to a MappedPacketMetadata which determines the output port,
  // cos, etc of the packet (used for packet out at the server/switch side).
  virtual ::util::Status ParsePacketOutMetadata(
      const ::p4::v1::PacketMetadata& p4_packet_metadata,
      MappedPacketMetadata* mapped_packet_metadata) const;

  // Converts a given MappedPacketMetadata to a P4 PacketMetadata to be
  // added to the packet before sending it to switch (used for packet out at the
  // client/controller side).
  virtual ::util::Status DeparsePacketOutMetadata(
      const MappedPacketMetadata& mapped_packet_metadata,
      ::p4::v1::PacketMetadata* p4_packet_metadata) const;

  // Converts a P4 PacketMetadata received from an incoming packet from
  // switch to a MappedPacketMetadata which determines the input port,
  // cos, etc of the packet (used for packet in at the client/controller side).
  virtual ::util::Status ParsePacketInMetadata(
      const ::p4::v1::PacketMetadata& p4_packet_metadata,
      MappedPacketMetadata* mapped_packet_metadata) const;

  // Fills in the MappedField for the associated table_id & field_id. Returns
  // ERR_ENTRY_NOT_FOUND if the lookup fails.
  virtual ::util::Status MapMatchField(int table_id, uint32 field_id,
                                       MappedField* mapped_field) const;

  // Lookup the P4 config Table from the given table_id.
  virtual ::util::Status LookupTable(int table_id,
                                     ::p4::config::v1::Table* table) const;

  // These methods control updates to P4 tables with static entries, i.e.
  // tables that contain "const entries" in the P4 program.  By default,
  // table mapping is disabled for P4 Runtime write requests that refer to
  // static tables.  In some circumstances, such as a P4PipelineConfig update,
  // entries in static tables may change.  Surrounding the changes with the
  // enable/disable operations below allows P4TableMapper to map flows
  // for the static entry updates relative to the new P4PipelineConfig.
  //
  // When a new P4 pipeline config push occurs, deletion of affected static
  // table entries must occur while the old pipeline config is still in effect,
  // and changes to any new or modified static entries must occur after the new
  // pipeline config is fully committed.  The following steps explain the
  // overall sequence for committing a pipeline config relative to one device
  // with static table entry changes:
  //  - The switch P4 service receives a new P4PipelineConfig push via
  //    P4Runtime RPC.  It breaks out the requests by device_id and proceeds
  //    with the following steps.
  //  - The switch implementation calls HandlePrePushStaticEntryChanges
  //    with new_static_config referring to static_table_entries from the
  //    P4PipelineConfig in the push RPC.  HandlePrePushStaticEntryChanges
  //    generates output in out_request to indicate which existing entries,
  //    if any, need to be deleted to achieve the new P4PipelineConfig.
  //  - The switch implementation determines whether the static table
  //    deletions can be achieved in conjunction with any other P4PipelineConfig
  //    changes and ultimately decides whether the P4 reconfiguration can
  //    proceed without a reboot.  The remaining steps below assume no reboot.
  //  - EnableStaticTableUpdates allows table mapping for the static entry
  //    pre-push changes.
  //  - The switch implementation uses MapFlowEntry to translate the flows
  //    for the changed entries (which are generally deletions), then removes
  //    the flows from their physical tables.
  //  - DisableStaticTableUpdates turns off static entry changes.
  //  - Once old static entries that may depend on the existing P4 configuration
  //    have been purged, the switch implementation can proceed to make any
  //    table, action, or other P4 resource changes that new P4PipelineConfig
  //    requires by calling PushForwardingPipelineConfig in this instance
  //    of P4TableMapper.  It should also push the new pipeline config
  //    to any other P4PipelineConfig-aware Stratum objects.
  //  - After the new pipeline config is committed, the switch implementation
  //    calls HandlePostPushStaticEntryChanges with new_static_config
  //    referring to static_table_entries from the new P4PipelineConfig in the
  //    push RPC.  HandlePostPushStaticEntryChanges generates output in
  //    out_request to indicate which entries from new_static_config did not
  //    exist in or have been modified since previously pushed configs.
  //  - The switch implementation executes another EnableStaticTableUpdates/
  //    MapFlowEntry/DisableStaticTableUpdates sequence to translate the flows
  //    for these static entry changes, then programs the flows into their
  //    physical tables.
  virtual void EnableStaticTableUpdates();
  virtual void DisableStaticTableUpdates();
  virtual ::util::Status HandlePrePushStaticEntryChanges(
      const ::p4::v1::WriteRequest& new_static_config,
      ::p4::v1::WriteRequest* out_request);
  virtual ::util::Status HandlePostPushStaticEntryChanges(
      const ::p4::v1::WriteRequest& new_static_config,
      ::p4::v1::WriteRequest* out_request);

  // IsTableStageHidden determines whether the input table_id maps to a
  // "HIDDEN" pipeline stage.  The "HIDDEN" stage applies to P4 logical tables
  // that have no equivalent physical table in the forwarding pipeline.  The
  // Stratum implementation absorbs the hidden table actions into related
  // physical tables.  IsTableStageHidden returns a TriState value:
  //  TRI_STATE_TRUE - the table is valid and maps to a hidden pipeline stage.
  //  TRI_STATE_FALSE - the table is valid and maps to a physical stage.
  //  TRI_STATE_UNKNOWN - the input table_id is unknown.
  // It is the caller's responsibility to determine whether TRI_STATE_UNKNOWN
  // is an error.
  virtual TriState IsTableStageHidden(int table_id) const;

  // This mutator is primarily for unit tests that need to use a
  // P4StaticEntryMapperMock.  P4TableMapper takes ownership of
  // the mapper instance.
  void set_static_entry_mapper(P4StaticEntryMapper* mapper) {
    static_entry_mapper_.reset(mapper);
  }

  // Factory function for creating an instance of the P4TableMapper.
  static std::unique_ptr<P4TableMapper> CreateInstance();

  // P4TableMapper is neither copyable nor movable.
  P4TableMapper(const P4TableMapper&) = delete;
  P4TableMapper& operator=(const P4TableMapper&) = delete;

 protected:
  // Default constructor - protected for mocks.
  P4TableMapper();

 private:
  // The P4GlobalIDTableMap provides mapping data for translating table
  // updates.  Its values point to descriptor data from the p4_pipeline_config_.
  // Its keys are global P4 object IDs, so a direct lookup of a mapping
  // descriptor is possible with object IDs from a TableWriteRequest RPC.
  // P4 tables and actions have global object IDs, i.e. every table and action
  // in a given P4Info specification has a unique ID.
  typedef absl::flat_hash_map<int, const P4TableMapValue*> P4GlobalIDTableMap;

  // These types support a map for field value conversion:
  // P4FieldConvertKey - IDs in P4Info MatchFields have unique scope within the
  //     enclosing table, so the lookup key is a combination of table ID and
  //     match field ID.
  // P4FieldConvertValue - Different tables can match on the same field in
  //     different ways, i.e. EXACT vs. LPM.  The map value indicates the
  //     table-dependent match attributes in conversion_entry and the type of
  //     field being matched.
  // P4FieldConvertByTable - This is the map for determining the type of match
  //     translation for a given table and match field combination.
  typedef std::pair<int, uint32> P4FieldConvertKey;
  struct P4FieldConvertValue {
    P4FieldDescriptor::P4FieldConversionEntry conversion_entry;
    MappedField mapped_field;
  };
  typedef std::map<P4FieldConvertKey, P4FieldConvertValue>
      P4FieldConvertByTable;

  // P4FieldConvertKey generators.
  inline static P4FieldConvertKey MakeP4FieldConvertKey(int table_id,
                                                        uint32 match_field_id) {
    return std::make_pair(table_id, match_field_id);
  }
  inline static P4FieldConvertKey MakeP4FieldConvertKey(
      const ::p4::config::v1::Table& table,
      const ::p4::v1::FieldMatch& match_field) {
    return MakeP4FieldConvertKey(table.preamble().id(), match_field.field_id());
  }
  inline static P4FieldConvertKey MakeP4FieldConvertKey(
      const ::p4::config::v1::Table& table,
      const ::p4::config::v1::MatchField& match_field) {
    return MakeP4FieldConvertKey(table.preamble().id(), match_field.id());
  }

  // This private class helps P4TableMapper with the details of action
  // parameter mapping.  A P4ActionParamMapper instance typically lives for
  // the duration of one set of P4Info.  Thus, there is an AddAction method
  // to handle actions defined by the P4Info in a config push, but no
  // corresponding DeleteAction method is necessary.  If the P4Info changes,
  // P4TableMapper simply deletes the old P4ActionParamMapper and
  // creates a new one to deal with the new P4Info.
  class P4ActionParamMapper {
   public:
    // The injected parameters must remain in scope
    // through the life of a P4ActionParamMapper instance.
    P4ActionParamMapper(const P4InfoManager& p4_info_manager,
                        const P4GlobalIDTableMap& p4_global_table_map,
                        const P4PipelineConfig& p4c_pipeline_cfg);
    virtual ~P4ActionParamMapper() {}

    // Creates a mapping entry for the given action and table ID inputs.
    // AddAction assumes that the caller has already verified that action_id
    // is valid in the input table_id's P4Info.  AddAction creates mapping
    // entries for each of action_id's parameters.
    ::util::Status AddAction(int table_id, int action_id);

    // Maps the PI action parameter in param to new modify_fields and/or
    // primitives in mapped_action.
    ::util::Status MapActionParam(int action_id,
                                  const ::p4::v1::Action::Param& param,
                                  MappedAction* mapped_action) const;

    // Maps the action's constant assignments to header fields or parameters
    // for other actions.
    ::util::Status MapActionConstants(int action_id,
                                      MappedAction* mapped_action) const;

    // Returns an OK status if action_id is a permissible action for the
    // input table_id.
    ::util::Status IsActionInTableInfo(int table_id, int action_id) const;

    // P4ActionParamMapper is neither copyable nor movable.
    P4ActionParamMapper(const P4ActionParamMapper&) = delete;
    P4ActionParamMapper& operator=(const P4ActionParamMapper&) = delete;

   private:
    // This struct tells how to map a PI action parameter to its encoding
    // in CommonFlowEntry.  AddAction creates an entry for each parameter from
    // data in P4Info and the action descriptor:
    //  field_types - for parameters the action assigns to header fields of
    //      various types.  Example: an action parameter that rewrites the
    //      destination MAC address during L3 forwarding uses field_types
    //      with a single P4_FIELD_TYPE_ETH_DST entry.
    //  bit_width - expresses the width of the action parameter and indicates
    //      how the parameter's PI-encoded value is converted to a value
    //      in CommonFlowEntry.
    //  param_descriptor - pointer to parameter's data in action descriptor.
    struct P4ActionParamEntry {
      P4ActionParamEntry()
          : field_types{}, bit_width(0), param_descriptor(nullptr) {}

      std::vector<P4FieldType> field_types;
      int bit_width;
      const P4ActionDescriptor::P4ActionInstructions* param_descriptor;
    };

    // The P4ActionParamMap provides a P4ActionParamEntry value that
    // P4ActionParamMapper can look up for each action parameter.  The key
    // is a combination of the action ID and the action parameter ID.  The
    // action ID is globally unique, but the parameter ID is unique only
    // within the scope of its action.
    typedef std::map<std::pair<int, int>, P4ActionParamEntry> P4ActionParamMap;

    // The P4ActionConstantMap supports actions that use constants to assign
    // fields or pass to other actions.  The action ID is the key, and the
    // value is a container of entries that define the constant assignments.
    typedef std::vector<P4ActionParamEntry> P4ActionConstants;
    typedef absl::flat_hash_map<int, P4ActionConstants> P4ActionConstantMap;

    // Updates param_entry with target header field assignments from
    // param_entry's param_descriptor.  In most cases, the param_descriptor
    // represents a real action parameter.  It can also represent constant
    // value assignments to header fields.   AddAssignedFields expects
    // param_entry's param_descriptor to be non-NULL on input.
    ::util::Status AddAssignedFields(P4ActionParamEntry* param_entry) const;

    // MapActionAssignment does common work for assigning action parameters
    // or constants to fields or passing their value to action primitives.
    static void MapActionAssignment(
        const P4ActionParamEntry& param_map_entry,
        const P4ActionFunction::P4ActionFields& param_value,
        MappedAction* mapped_action);

    // Searches action_descriptor for a parameter matching param_name and
    // returns the matching parameter descriptor data.
    static ::util::StatusOr<const P4ActionDescriptor::P4ActionInstructions*>
    FindParameterDescriptor(const std::string& param_name,
                            const P4ActionDescriptor& action_descriptor);

    // Converts a PI-encoded parameter value to the appropriate type for
    // the value output.
    static void ConvertParamValue(const ::p4::v1::Action::Param& param,
                                  int bit_width,
                                  P4ActionFunction::P4ActionFields* value);

    // The constructor injects these members, which are not owned by this class.
    const P4InfoManager& p4_info_manager_;
    const P4GlobalIDTableMap& p4_global_table_map_;
    const P4PipelineConfig& p4_pipeline_config_;

    // This member contains details for mapping each action parameter by ID.
    P4ActionParamMap action_param_map_;

    // This member contains details for mapping an action's constant value
    // assignments.
    P4ActionConstantMap action_constant_map_;

    // The valid_table_actions_ set contains all valid table ID and action ID
    // pairs, i.e. the action ID is defined in P4Info as one of the table's
    // possible actions.  The first pair member is the table ID, and the second
    // member is the action ID.
    std::set<std::pair<int, int>> valid_table_actions_;
  };

  // Creates the global_id_table_map_ entry for the object represented by the
  // input preamble.
  ::util::Status AddMapEntryFromPreamble(
      const ::p4::config::v1::Preamble& preamble);

  // Finds the object name string in the P4 object preamble.  The name becomes
  // the key for P4PipelineConfig table map lookups.  If no name is found, the
  // return string is empty.
  std::string GetMapperNameKey(const ::p4::config::v1::Preamble& preamble);

  // Validates all of the match fields in the table_entry from a P4Runtime
  // WriteRequest message.  The input table_p4_info provides information
  // about the expected match fields for the applicable table.  If the
  // P4Runtime request omits some match fields as "don't care" values,
  // PrepareMatchFields appends them to the all_match_fields output vector.
  // Upon successful return, all_match_fields combines the match fields
  // in the original WriteRequest with any additional don't care fields,
  // yielding the full set of match fields as specified by table_p4_info.
  ::util::Status PrepareMatchFields(
      const ::p4::config::v1::Table& table_p4_info,
      const ::p4::v1::TableEntry& table_entry,
      std::vector<::p4::v1::FieldMatch>* all_match_fields) const;

  // Processes the identified table and updates table-level flow_entry output.
  // Output always includes table_info with id, name, and type.  If the table's
  // P4Info contains annotations, they are also included in the output.  The
  // output may include internal match fields if they have been defined
  // in the P4PipelineConfig table map.
  ::util::Status ProcessTableID(const ::p4::config::v1::Table& table_p4_info,
                                int table_id,
                                CommonFlowEntry* flow_entry) const;

  // Processes one match_field from a table entry.  If successful, a new
  // MappedField will be added to flow_entry.
  ::util::Status ProcessMatchField(const ::p4::config::v1::Table& table_p4_info,
                                   const ::p4::v1::FieldMatch& match_field,
                                   CommonFlowEntry* flow_entry) const;

  // Processes the action from a table entry.  If successful, the
  // MappedAction will be populated in flow_entry.
  ::util::Status ProcessTableAction(
      const ::p4::config::v1::Table& table_p4_info,
      const ::p4::v1::TableAction& table_action,
      CommonFlowEntry* flow_entry) const;

  // These methods both handle action function processing.  The first one is
  // for actions in table updates.  The second one is for actions in action
  // profile updates.  Both of them produce mapped_action output when
  // successful.
  ::util::Status ProcessTableActionFunction(
      const ::p4::config::v1::Table& table_p4_info,
      const ::p4::v1::Action& action, MappedAction* mapped_action) const;
  ::util::Status ProcessProfileActionFunction(
      const ::p4::config::v1::ActionProfile& profile_p4_info,
      const ::p4::v1::Action& action, MappedAction* mapped_action) const;

  // Processes the action redirects specified by the action for the table.
  ::util::Status ProcessTableActionRedirects(
      const ::p4::config::v1::Table& table_p4_info,
      const ::p4::v1::Action& action, MappedAction* mapped_action) const;

  // Handles action function processing that is common to either a table entry
  // or an action profile update.  If successful, the output mapped_action will
  // be filled.
  ::util::Status ProcessActionFunction(const ::p4::v1::Action& action,
                                       MappedAction* mapped_action) const;

  // Evaluates the attributes in the table descriptor along with the current
  // state of static_table_updates_enabled_ to see if a mapping request is
  // allowed.
  ::util::Status IsTableUpdateAllowed(
      const ::p4::config::v1::Table& table_p4_info,
      const P4TableDescriptor& descriptor) const;

  // Clears all the entries in the containers that support the mapping process.
  void ClearMaps();

  // The p4_pipeline_config_ contains data to convert P4Info objects into
  // descriptor data for the mapping process.  This is the table map generated
  // by p4c and delivered to the switch via pipeline spec configuration.
  P4PipelineConfig p4_pipeline_config_;

  // Provides the mapping from P4 object IDs to action/table descriptors.
  P4GlobalIDTableMap global_id_table_map_;

  // This map facilitates table-dependent match field conversions.
  P4FieldConvertByTable field_convert_by_table_;

  // Map from packet in (out) metadata ID to the corresponding (type, bitwidth)
  // pair used for parsing the packet in (out) metadata. The ID and bitwidth of
  // metadata are available from P4Info and the type (P4FieldType) is found from
  // the output of the P4C backend.
  MetadataIdToTypeBitwidthMap packetin_metadata_id_to_type_bitwidth_pair_;
  MetadataIdToTypeBitwidthMap packetout_metadata_id_to_type_bitwidth_pair_;

  // Map from packet in (out) metadata type to the corresponding (ID, bitwidth)
  // pair used for deparsing the packet in (out) metadata. The ID and bitwidth
  // of metadata are available from P4Info and the type (P4FieldType) is found
  // from the output of the P4C backend.
  MetadataTypeToIdBitwidthMap packetin_metadata_type_to_id_bitwidth_pair_;
  MetadataTypeToIdBitwidthMap packetout_metadata_type_to_id_bitwidth_pair_;

  // The P4InfoManager provides access to the currently configured P4Info.
  std::unique_ptr<P4InfoManager> p4_info_manager_;

  // Refers to P4ActionParamMapper instance that helps with action parameter
  // mapping.
  std::unique_ptr<P4ActionParamMapper> param_mapper_;

  // Refers to the P4StaticEntryMapper instance that assists this instance
  // of P4TableMapper.
  std::unique_ptr<P4StaticEntryMapper> static_entry_mapper_;

  // This flag is true only when P4TableMapper allows updates to
  // flows in P4 tables with const entries.  These updates are allowed in
  // conjunction with forwarding pipeline pushes, but otherwise prohibited
  // during normal P4 runtime WriteRequest handling.
  bool static_table_updates_enabled_;

  // Logical node ID corresponding to the node/ASIC managed by this class
  // instance. Assigned on PushChassisConfig() and might change during the
  // lifetime of the class.
  uint64 node_id_;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_TABLE_MAPPER_H_
