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


#ifndef STRATUM_HAL_LIB_BCM_ACL_TABLE_H_
#define STRATUM_HAL_LIB_BCM_ACL_TABLE_H_

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/bcm/bcm_flow_table.h"
#include "stratum/hal/lib/p4/common_flow_entry.pb.h"
#include "stratum/public/proto/p4_annotation.pb.h"
#include "stratum/public/proto/p4_table_defs.pb.h"

namespace stratum {
namespace hal {
namespace bcm {

// AclTable represents the P4 view of an ACL table. Logical ACL tables may not
// be 1:1 matches to physical ACL tables in hardware.
class AclTable : public BcmFlowTable {
 public:
  //***************************************************************************
  //  Constructors
  //***************************************************************************
  AclTable(const ::p4::config::v1::Table& table, BcmAclStage stage, int priority)
      : BcmFlowTable(table),
        stage_(stage),
        match_fields_(),
        physical_table_id_(),
        max_entries_(table.size()),
        priority_(priority),
        udf_set_id_(-1),
        udf_match_fields_() {
    for (const auto& match_field : table.match_fields()) {
      match_fields_.insert(match_field.id());
    }
  }

  AclTable(const ::p4::config::v1::Table& table, P4Annotation::PipelineStage stage,
           int priority)
      : AclTable(table, P4PipelineToBcmAclStage(stage), priority) {}

  AclTable(const AclTable& other)
      : BcmFlowTable(other),
        stage_(other.stage_),
        match_fields_(other.match_fields_),
        physical_table_id_(other.physical_table_id_),
        max_entries_(other.max_entries_),
        priority_(other.priority_),
        udf_set_id_(other.udf_set_id_),
        udf_match_fields_(other.udf_match_fields_) {}

  AclTable(AclTable&& other)
      : BcmFlowTable(std::move(other)),
        stage_(other.stage_),
        match_fields_(std::move(other.match_fields_)),
        physical_table_id_(other.physical_table_id_),
        max_entries_(other.max_entries_),
        priority_(other.priority_),
        udf_set_id_(other.udf_set_id_),
        udf_match_fields_(std::move(other.udf_match_fields_)) {}

  //***************************************************************************
  //  Static translators
  //***************************************************************************
  // Translate the P4 Pipeline Stage used in annotations to a BcmAclStage used
  // by Bcm.
  static BcmAclStage P4PipelineToBcmAclStage(
      P4Annotation::PipelineStage p4_stage);

  //***************************************************************************
  //  Table Initializers
  //***************************************************************************
  // Set the physical table id.
  void SetPhysicalTableId(int id) { physical_table_id_ = id; }

  // Designate a match field as a UDF match field. This match field should be
  // translated to a UDF if used.
  ::util::Status MarkUdfMatchField(uint32 field, int udf_set_id);

  //***************************************************************************
  //  Accessors
  //***************************************************************************
  BcmAclStage Stage() const { return stage_; }
  int Priority() const { return priority_; }
  int Size() const { return max_entries_; }
  uint32 PhysicalTableId() const { return physical_table_id_; }
  const absl::flat_hash_set<uint32>& MatchFields() const {
    return match_fields_;
  }
  bool HasField(uint32 field) const { return match_fields_.count(field); }
  bool IsUdfField(uint32 field) const { return udf_match_fields_.count(field); }
  bool HasUdf() const { return !udf_match_fields_.empty(); }
  int UdfSetId() const { return udf_set_id_; }

  // Returns the BCM ACL ID for an entry in this table.
  // Returns ERR_ENTRY_NOT_FOUND if the entry does not exist in this table.
  // Returns ERR_NOT_INITIALIZED if the entry exists but no mapping is found.
  util::StatusOr<int> BcmAclId(const ::p4::v1::TableEntry& entry) const;

  //***************************************************************************
  //  Table Entry Management
  //***************************************************************************
  // Attempts to add the entry to this table.
  // Returns ERR_ENTRY_EXISTS if the entry already exists.
  // Returns ERR_NO_RESOURCE if the table is full.
  // Returns ERR_INVALID_PARAM if the entry contains an unsupported match field.
  util::Status InsertEntry(const ::p4::v1::TableEntry& entry) override {
    RETURN_IF_ERROR(DryRunInsertEntry(entry));
    return BcmFlowTable::InsertEntry(entry);
  }

  // Performs a dry-run of InsertEntry. Returns an error if the entry cannot be
  // inserted into the table. Returns util::OkStatus if it can.
  util::Status DryRunInsertEntry(const ::p4::v1::TableEntry& entry) const override;

  // Attempts to add the entry to this table with the provided Bcm ACL ID
  // mapping.
  // Returns ERR_ENTRY_EXISTS if the entry already exists.
  // Returns ERR_NO_RESOURCE if the table is full.
  util::Status InsertEntry(const ::p4::v1::TableEntry& entry, int bcm_acl_id);

  // Attempts to modify an existing entry in this table. Returns the original
  // entry on success.
  // Returns ERR_ENTRY_NOT_FOUND if a matching entry does not already exist.
  // Returns an error if the entry cannot be added.
  util::StatusOr<p4::v1::TableEntry> ModifyEntry(
      const ::p4::v1::TableEntry& entry) override {
    // Remove the entry, but don't remove the record in bcm_acl_id_map_.
    ASSIGN_OR_RETURN(p4::v1::TableEntry old_entry,
                     BcmFlowTable::DeleteEntry(entry));
    entries_.insert(entry);
    return old_entry;
  }

  // Attempts to set the Bcm ACL ID for an entry in this table.
  // Returns ERR_ENTRY_NOT_FOUND if the entry is not found.
  util::Status SetBcmAclId(const ::p4::v1::TableEntry& entry, int bcm_acl_id);

  // Attempts to delete the entry from this table.
  // Returns ERR_ENTRY_NOT_FOUND if a matching entry does not already exist.
  util::StatusOr<p4::v1::TableEntry> DeleteEntry(
      const ::p4::v1::TableEntry& entry) override {
    // We aren't interested in the return for erase since it's possible nobody
    // ever set the associated Bcm ACL ID.
    bcm_acl_id_map_.erase(entry);
    return BcmFlowTable::DeleteEntry(entry);
  }

 private:
  //***************************************************************************
  //  Members
  //***************************************************************************
  // The ACL stage for this table.
  BcmAclStage stage_;
  // Available qualifers for this table stored as match field IDs.
  absl::flat_hash_set<uint32> match_fields_;
  // The BCM id this table belongs to.
  uint32 physical_table_id_;
  // The maximum number of entries that can be programmed into the logical table
  // during runtime. This does not include the default_action entry.
  // Type is int64 to match ::p4::config::v1::Table.size
  int max_entries_;
  // Relative table priority. This is generated by the stack.
  int16 priority_;
  // ID of the UDF set used by this table. This value is only valid if
  // ufd_match_fields_ is not empty.
  int udf_set_id_;
  // The set of match field IDs in this table that use UDFs. This is a subset of
  // match_fields_.
  absl::flat_hash_set<uint32> udf_match_fields_;
  // Mapping from entries to their respective Bcm ACL IDs.
  absl::flat_hash_map<::p4::v1::TableEntry, uint32, TableEntryHash, TableEntryEqual>
      bcm_acl_id_map_;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_ACL_TABLE_H_
