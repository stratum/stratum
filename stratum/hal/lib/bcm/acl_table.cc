// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "stratum/hal/lib/bcm/acl_table.h"
#include "util/gtl/map_util.h"

namespace stratum {
namespace hal {
namespace bcm {

// Convert the P4 Pipeline Stage used in annotations to a BcmAclStage used in
// the hardware.
BcmAclStage AclTable::P4PipelineToBcmAclStage(
    P4Annotation::PipelineStage p4_stage) {
  switch (p4_stage) {
    case P4Annotation::INGRESS_ACL:
      return BCM_ACL_STAGE_IFP;
    case P4Annotation::VLAN_ACL:
      return BCM_ACL_STAGE_VFP;
    case P4Annotation::EGRESS_ACL:
      return BCM_ACL_STAGE_EFP;
    default:
      return BCM_ACL_STAGE_UNKNOWN;
  }
}

::util::Status AclTable::MarkUdfMatchField(uint32 field, int udf_set_id) {
    if (!HasField(field)) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "ACL Table " << Id()
             << " does not contain match field: " << field
             << ". Cannot mark field as UDF.";
    }
    if (udf_set_id < 0) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid UDF set id: " << udf_set_id << ".";
    }
    if (udf_match_fields_.empty()) {
      udf_set_id_ = udf_set_id;
    } else if (udf_set_id_ != udf_set_id) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "ACL Table " << Id() << " already uses UDF set " << udf_set_id_
             << ". Cannot designate a UDF match field from UDF set "
             << udf_set_id << ".";
    }
    udf_match_fields_.insert(field);
    return ::util::OkStatus();
}

util::StatusOr<int> AclTable::BcmAclId(const ::p4::TableEntry& entry) const {
  // Search for the entry.
  const auto iter = bcm_acl_id_map_.find(entry);
  if (iter != bcm_acl_id_map_.end()) {
    return iter->second;
  }
  // Check if the table entry exists.
  if (!HasEntry(entry)) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << TableStr()
           << " does not contain TableEntry: " << entry.ShortDebugString()
           << ".";
  }
  // If the entry exists, the Bcm ACL ID is uninitialized.
  return MAKE_ERROR(ERR_NOT_INITIALIZED)
         << TableStr() << " has no BcmAclId associated with TableEntry: "
         << entry.ShortDebugString() << ".";
}

util::Status AclTable::DryRunInsertEntry(const ::p4::TableEntry& entry) const {
  const auto result = entries_.find(entry);
  // Duplicate entry check.
  if (result != entries_.end()) {
    return MAKE_ERROR(ERR_ENTRY_EXISTS)
           << TableStr()
           << " contains duplicate of TableEntry: " << entry.ShortDebugString()
           << ". Matching TableEntry: " << result->ShortDebugString() << ".";
  }
  // Table capacity check.
  if (EntryCount() == max_entries_) {
    return MAKE_ERROR(ERR_TABLE_FULL) << TableStr() << " is full.";
  }
  if (EntryCount() > max_entries_)
    return MAKE_ERROR(ERR_INTERNAL)
           << "Unexpected scenario in " << TableStr() << ": EntryCount ("
           << EntryCount() << ") > max_entries_ (" << max_entries_
           << "). There is a bug in AclTable bookkeeping.";
  // Match fields check.
  for (const auto& match : entry.match()) {
    if (match_fields_.count(match.field_id()) == 0) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << TableStr() << " does not contain field <" << match.field_id()
             << "> from TableEntry: " << entry.ShortDebugString() << ".";
    }
  }
  return BcmFlowTable::DryRunInsertEntry(entry);
}

util::Status AclTable::InsertEntry(const ::p4::TableEntry& entry,
                                   int bcm_acl_id) {
  RETURN_IF_ERROR(InsertEntry(entry));
  RETURN_IF_ERROR(SetBcmAclId(entry, bcm_acl_id));
  return util::OkStatus();
}

util::Status AclTable::SetBcmAclId(const ::p4::TableEntry& entry,
                                   int bcm_acl_id) {
  if (!HasEntry(entry)) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << TableStr()
           << " does not contain TableEntry: " << entry.ShortDebugString()
           << ".";
  }
  auto iter = bcm_acl_id_map_.find(entry);
  if (iter != bcm_acl_id_map_.end()) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Unexpected scenario in " << TableStr()
           << ": Leftover Bcm ACL ID <" << iter->second
           << "> found for TableEntry: " << entry.ShortDebugString() << ".";
  }
  bcm_acl_id_map_[entry] = bcm_acl_id;
  return util::OkStatus();
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
