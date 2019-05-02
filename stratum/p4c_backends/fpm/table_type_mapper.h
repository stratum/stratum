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

// The TableTypeMapper operates on P4 tables in fixed pipeline stages and
// attempts to determine additional P4TableDescriptor details from a
// table's match fields, pipeline stage, and action outputs.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TABLE_TYPE_MAPPER_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TABLE_TYPE_MAPPER_H_

#include <string>

#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/hal/lib/p4/p4_pipeline_config.host.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.host.pb.h"
#include "stratum/public/proto/p4_table_defs.host.pb.h"

namespace stratum {
namespace p4c_backends {

// A TableTypeMapper typically runs near the end of p4c backend processing,
// after the backend has created the P4Info, assigned table pipeline stages,
// decoded actions, determined field types, and populated the P4PipelineConfig
// to the fullest extent possible.  At this point, a TableTypeMapper instance
// executes its ProcessTables method and determines whether it can provide
// any additional P4PipelineConfig data.
class TableTypeMapper {
 public:
  TableTypeMapper();
  virtual ~TableTypeMapper() {}

  // Iterates over all the tables known to p4_info_manager, evaluates the
  // p4_pipeline_config descriptors pertaining to each table and its associated
  // actions and fields, and appends any additional table descriptor data that
  // may be useful to the Stratum switch stack.  The ProcessTables focus is
  // on table types for fixed pipeline stages on the target.  By the time
  // ProcessTables runs, the P4PipelineConfig and P4Info have enough data
  // for the switch stack to manage ACL-based tables.
  void ProcessTables(const hal::P4InfoManager& p4_info_manager,
                     hal::P4PipelineConfig* p4_pipeline_config);

  // TableTypeMapper is neither copyable nor movable.
  TableTypeMapper(const TableTypeMapper&) = delete;
  TableTypeMapper& operator=(const TableTypeMapper&) = delete;

 private:
  // Evaluates action_descriptor assignments that give hints regarding the
  // table type.  For example, if an action assigns an L2 multicast group,
  // then the input descriptor is part of an L2 multicast table.
  void GetL2TableTypeFromAction(
      const hal::P4ActionDescriptor& action_descriptor,
      const hal::P4PipelineConfig& p4_pipeline_config);

  // Sets new_table_type_ to proposed_table_type if and only if there are no
  // conflicts detected between the proposed value and the current provisional
  // value.  If conflicts occur, new_table_type_ reverts to P4_TABLE_UNKNOWN.
  void ProposeNewTableType(P4TableType proposed_table_type);

  // These members record the current status of table type determination as
  // ProcessTables iterates over each table's action assignments:
  //  new_table_type_ - tracks the type that will be assigned to the table
  //      based on the current state.
  //  found_table_type_ - records the first table type inferred from an action
  //      assignment; used by ProposeNewTableType for conflict detection.
  P4TableType new_table_type_;
  P4TableType found_table_type_;

  // Stores the name of the table that is being processed by ProcessTables.
  std::string current_table_name_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TABLE_TYPE_MAPPER_H_
