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

// The TableMapGenerator in the Hercules switch p4c backend accumulates
// P4PipelineConfig table map entries as the backend identifies fields,
// matches, tables, etc.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TABLE_MAP_GENERATOR_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TABLE_MAP_GENERATOR_H_

#include <memory>
#include <set>
#include <string>

#include "stratum/hal/lib/p4/p4_pipeline_config.host.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.host.pb.h"
#include "stratum/public/proto/p4_table_defs.host.pb.h"
#include "absl/base/integral_types.h"

namespace stratum {
namespace p4c_backends {

// A single instance of TableMapGenerator produces the table map output for
// a Hercules switch p4c backend.
class TableMapGenerator {
 public:
  TableMapGenerator();
  virtual ~TableMapGenerator();

  // The next five methods manage field_descriptor entries in the
  // generated P4PipelineConfig table map:
  //  AddField - Adds a new field_descriptor with the given name to the
  //      generated map.  If the field already exists, no action occurs.
  //  SetFieldType - Sets the type field in the existing field_descriptor for
  //      the input field_name.  See additional SetFieldAttributes notes
  //      about changing types.
  //  SetFieldAttributes - Works like SetFieldType, but also includes the
  //      field's most common field descriptor attributes.  If the caller
  //      does not know the field_type or header_type, either type can be
  //      passed as unknown (P4_FIELD_TYPE_UNKNOWN or P4_HEADER_UNKNOWN), and
  //      any pre-existing type in the field descriptor will remain intact.
  //      If the backend calls SetFieldType and/or SetFieldAttributes multiple
  //      times with any other input type for the same field, the most recent
  //      type takes precedence.  If the backend calls SetFieldAttributes
  //      multiple times with different widths or offsets for the same field,
  //      TableMapGenerator logs an error but uses the most recent values.
  //      TODO(teverman): Changing field type should probably be treated as
  //      some kind of compiler or P4 program bug, but the current behavior
  //      precedent has been in place for some time.
  //  SetFieldLocalMetadataFlag - Sets the is_local_metadata field in the
  //      existing field_descriptor for the input field_name.
  //  SetFieldValueSet - Adds parser value set attributes in the existing
  //      field descriptor for the input field_name.  The new attributes
  //      override any existing value set attributes.
  //  AddFieldMatch - Adds a new match type to an existing field descriptor.
  //      The value of match_type is one of "exact", "lpm", "ternary", or
  //     "selector", as identified by the field's IR data.  TableMapGenerator
  //     indicates unsupported match types via p4c's internal error reporter.
  //  ReplaceFieldDescriptor - replaces the entire field_descriptor for an
  //      existing field.
  // A typical usage for a backend extension is:
  //  const std::string field_name = <Field found in the P4 IR>;
  //  table_map_generator.AddField(field_name);
  //  P4FieldType field_type = <Type of field from further IR processing>;
  //  table_map_generator.SetFieldType(field_name, field_type);
  //  for (<uses of field as match key>) {
  //    table_map_generator.AddFieldMatch(
  //        field_name, <match key type>, <match key width>);
  //  }
  // SetFieldType and AddFieldMatch can be called in either order.  In some
  // cases, the backend may identify the match keys before determining the
  // type of field.
  virtual void AddField(const std::string& field_name);
  virtual void SetFieldType(const std::string& field_name, P4FieldType type);
  virtual void SetFieldAttributes(const std::string& field_name,
                                  P4FieldType field_type,
                                  P4HeaderType header_type,
                                  uint32_t bit_offset, uint32_t bit_width);
  virtual void SetFieldLocalMetadataFlag(const std::string& field_name);
  virtual void SetFieldValueSet(const std::string& field_name,
                                const std::string& value_set_name,
                                P4HeaderType header_type);
  virtual void AddFieldMatch(const std::string& field_name,
                             const std::string& match_type, int bit_width);
  virtual void ReplaceFieldDescriptor(
      const std::string& field_name,
      const hal::P4FieldDescriptor& new_descriptor);

  // The next set of methods manages action_descriptor entries in the
  // generated P4PipelineConfig table map:
  //  AddAction -  Adds a new action_descriptor with the given name to the
  //      generated map.  Due to recursion in the IR, it is permissible to add
  //      the same action more than once.  The second add operation does not
  //      change any existing data in the action_descriptor.
  //  AssignActionSourceValueToField - Adds a field assignment to an existing
  //      action_descriptor, assigning the source_value oneof from the input
  //      P4AssignSourceValue message to the given field_name.
  //  AssignActionParameterToField - Adds a field assignment to an existing
  //      action_descriptor, assigning the action parameter identified by
  //      param_name to the given field_name.
  //  AssignHeaderToHeader - Adds a field assignment to an existing
  //      action_descriptor, copying the source header to the destination
  //      header.
  //  AddDropPrimitive - adds the primitive drop action to an existing
  //      action_descriptor.
  //  AddNopPrimitive - adds the primitive nop action to an existing
  //      action_descriptor.
  //  AddMeterColorAction - adds the specified color_action to an existing
  //      action_descriptor.
  //  AddMeterColorActionsFromString - provides an alternate form of
  //      AddMeterColorAction where color_actions is a string (typically
  //      from an IR::MeterColorStatement node) with a text-encoded
  //      hal::P4ActionDescriptor containing one or more P4MeterColorActions
  //      to add to action_name's descriptor.
  //  AddTunnelAction - adds the specified tunnel_action to an existing
  //      action_descriptor.
  //  ReplaceActionDescriptor - replaces the entire action_descriptor for an
  //      existing action.
  // Usage depends on the type of action.  The backend calls AddAction when it
  // encounters an action in the IR.  Therafter, it uses the other methods to
  // define the action behavior according to the IR definition.
  virtual void AddAction(const std::string& action_name);
  virtual void AssignActionSourceValueToField(
      const std::string& action_name,
      const P4AssignSourceValue& source_value,
      const std::string& field_name);
  virtual void AssignActionParameterToField(
      const std::string& action_name, const std::string& param_name,
      const std::string& field_name);
  virtual void AssignHeaderToHeader(
      const std::string& action_name,
      const P4AssignSourceValue& source_header,
      const std::string& destination_header);
  virtual void AddDropPrimitive(const std::string& action_name);
  virtual void AddNopPrimitive(const std::string& action_name);
  virtual void AddMeterColorAction(
      const std::string& action_name,
      const hal::P4ActionDescriptor::P4MeterColorAction& color_action);
  virtual void AddMeterColorActionsFromString(
      const std::string& action_name, const std::string& color_actions);
  virtual void AddTunnelAction(
      const std::string& action_name,
      const hal::P4ActionDescriptor::P4TunnelAction& tunnel_action);
  virtual void ReplaceActionDescriptor(
      const std::string& action_name,
      const hal::P4ActionDescriptor& new_descriptor);

  // The next set of methods manages table_descriptor entries in the
  // generated P4PipelineConfig table map:
  //  AddTable - Adds a new table_descriptor with the given name to the
  //      generated map.  If the table already exists, no action occurs.
  //  SetTableType - Sets the type field in the existing table_descriptor for
  //      the input table_name.  If the backend calls SetTableType multiple
  //      times for the same table, the most recent type takes precedence.
  //  SetTableStaticEntriesFlag - Sets the has_static_entries flag in the
  //      table descriptor for the input table_name.
  //  SetTableValidHeaders - Replaces the valid_headers fields in the
  //      table descriptor with the P4HeaderType values corresponding to
  //      the input header names.  SetTableValidHeaders finds the P4HeaderType
  //      from existing header descriptor entries, ignoring any headers
  //      with missing header descriptors.
  virtual void AddTable(const std::string& table_name);
  virtual void SetTableType(const std::string& table_name, P4TableType type);
  virtual void SetTableStaticEntriesFlag(const std::string& table_name);
  virtual void SetTableValidHeaders(const std::string& table_name,
                                    const std::set<std::string>& header_names);

  // The next set of methods manages header_descriptor entries in the
  // generated P4PipelineConfig table map:
  //  AddHeader - Adds a new header_descriptor with the given name to the
  //      generated map.  If the header already exists, no action occurs.
  //  SetHeaderAttributes - Sets the header informnation in the existing table
  //      map header_descriptor for the input header name.  The depth parameter
  //      defines the header depth within an encap/decap tunnel or a header
  //      stack.  If the caller does not know the depth, it should set
  //      the depth parameter to 0, and the existing depth value will be
  //      unchanged.
  virtual void AddHeader(const std::string& header_name);
  virtual void SetHeaderAttributes(
      const std::string& header_name, P4HeaderType type, int32 depth);

  // This method adds a P4ActionDescriptor for an internally-generated action
  // to the P4PipelineConfig output.  An internal action is an action that
  // the p4c backend generates by merging multiple P4 actions.  If the same
  // action_name is added multiple times, AddInternalAction logs an error but
  // keeps the most recent internal_descriptor data.
  virtual void AddInternalAction(
      const std::string& action_name,
      const hal::P4ActionDescriptor& internal_descriptor);

  // Accessor for generated map.
  virtual const hal::P4PipelineConfig& generated_map() const {
    return *generated_map_;
  }

  // TableMapGenerator is neither copyable nor movable.
  TableMapGenerator(const TableMapGenerator&) = delete;
  TableMapGenerator& operator=(const TableMapGenerator&) = delete;

 private:
  // Finds the named action_descriptor in generated_map_.
  hal::P4ActionDescriptor* FindActionDescriptor(const std::string& action_name);

  // Searches the input action_descriptor for a P4MeterColorAction with a set
  // of metered colors that matches the color set in color_action.  If a match
  // occurs, FindColorAction returns the index of the matching entry in
  // action_descriptor.color_actions(); otherwise the result is -1.
  int FindColorAction(
      const hal::P4ActionDescriptor& action_descriptor,
      const hal::P4ActionDescriptor::P4MeterColorAction& color_action) const;

  // This pointer refers to the P4PipelineConfig that this TableMapGenerator is
  // producing.
  std::unique_ptr<hal::P4PipelineConfig> generated_map_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TABLE_MAP_GENERATOR_H_
