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


#include "stratum/hal/lib/bcm/bcm_acl_manager.h"

#include <iterator>

#include "stratum/hal/lib/bcm/acl_table.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/p4_annotation.pb.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_join.h"
#include "stratum/glue/gtl/map_util.h"

namespace stratum {
namespace hal {
namespace bcm {

namespace {

constexpr BcmSdkInterface::AclControl kDefaultAclControl = {
    // Enable all for external ports.
    {true, true, true, true},     // extern_port_flags.{vfp, ifp, efp, apply}
    // Disable all for internal ports.
    {false, false, false, true},  // intern_port_flags.{vfp, ifp, efp, apply}
    // Disable EFP for CPU ports.
    {true, true, false, true},    // cpu_port_flags.{vfp, ifp, efp, apply}
    // Enable instra-slice double wide tables.
    {true, true},                 // intra_double_wide_enable.{enable, apply}
    // Enable stats read through.
    {true, true}                  // stats_read_through_enable.{enable, apply}
};

// Return the table reference at the root of a p4 control statement.
bool GetStatementTableReference(const P4ControlStatement& statement,
                                P4ControlTableRef* table_reference) {
  switch (statement.statement_case()) {
    case P4ControlStatement::kApply:
      *table_reference = statement.apply();
      return true;
    case P4ControlStatement::kBranch:
      if (statement.branch().condition().has_hit()) {
        *table_reference = statement.branch().condition().hit();
        return true;
      }
      break;
    case P4ControlStatement::kDrop:
    case P4ControlStatement::kReturn:
    case P4ControlStatement::kExit:
    case P4ControlStatement::kFixedPipeline:
    case P4ControlStatement::kOther:
    case P4ControlStatement::STATEMENT_NOT_SET:
      // At this point, we have a statement that isn't based on a table.
      break;
  }
  return false;
}

}  // namespace

BcmAclManager::BcmAclManager(BcmChassisManager* bcm_chassis_manager,
                             BcmTableManager* bcm_table_manager,
                             BcmSdkInterface* bcm_sdk_interface,
                             P4TableMapper* p4_table_mapper, int unit)
    : initialized_(false),
      bcm_chassis_manager_(CHECK_NOTNULL(bcm_chassis_manager)),
      bcm_table_manager_(CHECK_NOTNULL(bcm_table_manager)),
      bcm_sdk_interface_(CHECK_NOTNULL(bcm_sdk_interface)),
      p4_table_mapper_(p4_table_mapper),
      node_id_(0),
      unit_(unit) {}

BcmAclManager::BcmAclManager()
    : initialized_(false),
      bcm_chassis_manager_(nullptr),
      bcm_table_manager_(nullptr),
      bcm_sdk_interface_(nullptr),
      p4_table_mapper_(nullptr),
      node_id_(0),
      unit_(-1) {}

BcmAclManager::~BcmAclManager() {}

::util::Status BcmAclManager::PushChassisConfig(const ChassisConfig& config,
                                                uint64 node_id) {
  node_id_ = node_id;  // Save node_id ASAP to ensure all the methods can refer
                       // to correct ID in the messages/errors.

  RETURN_IF_ERROR(OneTimeSetup())
      << "Failed to configure ACL hardware for node " << node_id
      << " (unit: " << unit_ << "): " << config.ShortDebugString() << ".";
  return ::util::OkStatus();
}

::util::Status BcmAclManager::VerifyChassisConfig(const ChassisConfig& config,
                                                  uint64 node_id) {
  if (node_id == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid node ID.";
  }
  if (node_id_ > 0 && node_id_ != node_id) {
    return MAKE_ERROR(ERR_REBOOT_REQUIRED)
           << "Detected a change in the node_id (" << node_id_ << " vs "
           << node_id << ").";
  }

  return ::util::OkStatus();
}

::util::Status BcmAclManager::PushForwardingPipelineConfig(
    const ::p4::ForwardingPipelineConfig& config) {
  // The pipeline config is stored as raw bytes in the p4_device_config.
  P4PipelineConfig p4_pipeline_config;
  if (!p4_pipeline_config.ParseFromString(config.p4_device_config())) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Failed to parse config.p4_device_config byte stream to "
           << "P4PipelineConfig: " << config.p4_device_config() << ".";
  }

  if (ProtoEqual(p4_pipeline_config, p4_pipeline_config_)) {
    LOG(INFO) << "Forwarding pipeline config is unchanged for node with ID "
              << node_id_ << " mapped to unit " << unit_ << ". Skipped!";
    return ::util::OkStatus();
  }
  p4_pipeline_config_ = p4_pipeline_config;

  // Clean all the ACL tables before applying the new config.
  // TODO: This should be replaced with a reconcile if the new
  // pipeline config is a superset of the old one.
  RETURN_IF_ERROR(ClearAllAclTables());

  // Grab all the ACL tables. These tables are organized by physical ACL tables.
  // We assume that each P4Control represents hardware-independent control
  // blocks (i.e. no ACL pipeline spans multiple control blocks).
  std::vector<PhysicalAclTable> physical_acl_tables;
  for (const P4Control& control : p4_pipeline_config.p4_controls()) {
    auto result = PhysicalAclTablesFromPipeline(control.main());
    RETURN_IF_ERROR_WITH_APPEND(result.status())
        << " Failed to set up acl pipelines for control: " << control.name()
        << ", type: " << control.type() << ".";
    physical_acl_tables.insert(physical_acl_tables.end(),
                               make_move_iterator(result.ValueOrDie().begin()),
                               make_move_iterator(result.ValueOrDie().end()));
  }

  // Install and update the ACL tables.
  for (PhysicalAclTable& physical_acl_table : physical_acl_tables) {
    ASSIGN_OR_RETURN(int physical_table_id,
                     InstallPhysicalTable(physical_acl_table));
    // Update the physical table ID for each AclTable.
    std::vector<uint32> acl_table_ids;  // For logging.
    for (AclTable& acl_table : physical_acl_table.logical_tables) {
      acl_table.SetPhysicalTableId(physical_table_id);
      acl_table_ids.push_back(acl_table.Id());
    }
    // Log the installation.
    LOG(INFO) << "P4 ACL Tables (" << absl::StrJoin(acl_table_ids, ", ")
              << ") installed as Physical ACL Table (" << physical_table_id
              << ").";
  }

  // Record the logical tables in BcmTableManager.
  for (PhysicalAclTable& physical_acl_table : physical_acl_tables) {
    auto& logical_tables = physical_acl_table.logical_tables;
    auto iter = std::make_move_iterator(logical_tables.begin());
    while (iter != std::make_move_iterator(logical_tables.end())) {
      RETURN_IF_ERROR(bcm_table_manager_->AddAclTable(*iter));
      ++iter;
    }
  }

  LOG(INFO) << "ACL Manager successfully pushed forwarding pipeline config to "
            << "node with ID " << node_id_ << " mapped to unit " << unit_
            << ".";

  return ::util::OkStatus();
}

::util::Status BcmAclManager::VerifyForwardingPipelineConfig(
    const ::p4::ForwardingPipelineConfig& config) {
  // TODO: Implement if needed.
  return ::util::OkStatus();
}

::util::Status BcmAclManager::Shutdown() {
  // TODO: Implement if needed.
  return ::util::OkStatus();
}

::util::Status BcmAclManager::InsertTableEntry(
    const ::p4::TableEntry& entry) const {
  VLOG(3) << "Inserting table entry " << entry.ShortDebugString();
  // Verify this entry can be added to the software state.
  ASSIGN_OR_RETURN(const AclTable* table,
                   bcm_table_manager_->GetReadOnlyAclTable(entry.table_id()));
  RETURN_IF_ERROR(table->DryRunInsertEntry(entry));

  // Convert the entry to a BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  RETURN_IF_ERROR_WITH_APPEND(bcm_table_manager_->FillBcmFlowEntry(
      entry, ::p4::Update::INSERT, &bcm_flow_entry))
      << " Failed to insert table entry: " << entry.ShortDebugString() << ".";

  // TODO: Implement stat coloring options.
  auto bcm_result =
      bcm_sdk_interface_->InsertAclFlow(unit_, bcm_flow_entry, true, false);
  RETURN_IF_ERROR_WITH_APPEND(bcm_result.status())
      << "\n"
      << "Failed to insert table entry: " << entry.ShortDebugString() << "\n"
      << "and bcm entry: " << bcm_flow_entry.ShortDebugString() << "\n"
      << "in unit " << unit_ << ".";
  RETURN_IF_ERROR_WITH_APPEND(
      bcm_table_manager_->AddAclTableEntry(entry, bcm_result.ValueOrDie()))
      << " ACL table entry was created but failed to record.";
  VLOG(3) << "Successfully inserted table entry " << entry.ShortDebugString()
          << " into unit " << unit_ << ".";
  return ::util::OkStatus();
}

::util::Status BcmAclManager::ModifyTableEntry(
    const ::p4::TableEntry& entry) const {
  VLOG(3) << "Modifying table entry: " << entry.ShortDebugString() << ".";
  ASSIGN_OR_RETURN(const AclTable* table,
                   bcm_table_manager_->GetReadOnlyAclTable(entry.table_id()));
  ASSIGN_OR_RETURN(int bcm_acl_id, table->BcmAclId(entry));

  // Convert: ::p4::TableEntry --> CommonFlowEntry --> BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  RETURN_IF_ERROR_WITH_APPEND(bcm_table_manager_->FillBcmFlowEntry(
      entry, ::p4::Update::MODIFY, &bcm_flow_entry))
      << " Failed to modify table entry: " << entry.ShortDebugString() << ".";

  // Perform the flow modification.
  RETURN_IF_ERROR_WITH_APPEND(
      bcm_sdk_interface_->ModifyAclFlow(unit_, bcm_acl_id, bcm_flow_entry))
      << " Failed to modify table entry: " << entry.ShortDebugString()
      << " as bcm entry: " << bcm_flow_entry.ShortDebugString() << ".";

  // Record the flow modification.
  RETURN_IF_ERROR_WITH_APPEND(bcm_table_manager_->UpdateTableEntry(entry));
  VLOG(3) << "Successfully modified ACL table entry: "
          << entry.ShortDebugString() << ".";
  return ::util::OkStatus();
}

::util::Status BcmAclManager::DeleteTableEntry(
    const ::p4::TableEntry& entry) const {
  VLOG(3) << "Deleting table entry: " << entry.ShortDebugString() << ".";
  ASSIGN_OR_RETURN(const AclTable* table,
                   bcm_table_manager_->GetReadOnlyAclTable(entry.table_id()));
  ASSIGN_OR_RETURN(int bcm_acl_id, table->BcmAclId(entry));
  RETURN_IF_ERROR_WITH_APPEND(
      bcm_sdk_interface_->RemoveAclFlow(unit_, bcm_acl_id))
      << "Failed to delete table entry: " << entry.ShortDebugString() << ".";
  RETURN_IF_ERROR(bcm_table_manager_->DeleteTableEntry(entry));
  return ::util::OkStatus();
}

::util::Status BcmAclManager::UpdateTableEntryMeter(
    const ::p4::DirectMeterEntry& meter) const {
  const ::p4::TableEntry& entry = meter.table_entry();
  ASSIGN_OR_RETURN(const AclTable* table,
                   bcm_table_manager_->GetReadOnlyAclTable(entry.table_id()));
  ASSIGN_OR_RETURN(int bcm_acl_id, table->BcmAclId(entry));

  // Transfer the meter configuration to BcmMeterConfig.
  BcmMeterConfig bcm_meter_config;
  RETURN_IF_ERROR(bcm_table_manager_->FillBcmMeterConfig(meter.config(),
                                                         &bcm_meter_config));
  // Set the meter configuration in hardware.
  RETURN_IF_ERROR(
      bcm_sdk_interface_->SetAclPolicer(unit_, bcm_acl_id, bcm_meter_config));

  // Update the meter configuration in software.
  RETURN_IF_ERROR(bcm_table_manager_->UpdateTableEntryMeter(meter));

  return ::util::OkStatus();
}

::util::Status BcmAclManager::GetTableEntryStats(
    const ::p4::TableEntry& entry, ::p4::CounterData* counter) const {
  if (counter == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null counter.";
  }
  ASSIGN_OR_RETURN(const AclTable* table,
                   bcm_table_manager_->GetReadOnlyAclTable(entry.table_id()));
  ASSIGN_OR_RETURN(int bcm_acl_id, table->BcmAclId(entry));

  BcmAclStats stats;
  RETURN_IF_ERROR_WITH_APPEND(
      bcm_sdk_interface_->GetAclStats(unit_, bcm_acl_id, &stats))
      << "Failed to obtain stats for table entry from hardware: "
      << entry.ShortDebugString();
  if (!stats.has_total()) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "Did not find total stat counter data for table entry: "
           << entry.ShortDebugString() << ".";
  }
  counter->set_byte_count(static_cast<int64>(stats.total().bytes()));
  counter->set_packet_count(static_cast<int64>(stats.total().packets()));
  return ::util::OkStatus();
}

std::unique_ptr<BcmAclManager> BcmAclManager::CreateInstance(
    BcmChassisManager* bcm_chassis_manager, BcmTableManager* bcm_table_manager,
    BcmSdkInterface* bcm_sdk_interface, P4TableMapper* p4_table_mapper,
    int unit) {
  return absl::WrapUnique(
      new BcmAclManager(bcm_chassis_manager, bcm_table_manager,
                        bcm_sdk_interface, p4_table_mapper, unit));
}

::util::Status BcmAclManager::OneTimeSetup() {
  if (!initialized_) {
    RETURN_IF_ERROR(bcm_sdk_interface_->InitAclHardware(unit_));
    RETURN_IF_ERROR(
        bcm_sdk_interface_->SetAclControl(unit_, kDefaultAclControl));
    initialized_ = true;
    LOG(INFO) << "ACL manager successfully configured ACLs for node with ID "
              << node_id_ << " mapped to unit " << unit_ << ".";
  }
  return ::util::OkStatus();
}

::util::Status BcmAclManager::ClearAllAclTables() {
  std::set<uint32> acl_table_ids = bcm_table_manager_->GetAllAclTableIDs();
  if (acl_table_ids.empty()) return util::OkStatus();
  // Remove all the ACL table entries from hardware & software.
  ::p4::ReadResponse response;
  std::vector<::p4::TableEntry*> all_acl_entries;
  RETURN_IF_ERROR(bcm_table_manager_->ReadTableEntries(acl_table_ids, &response,
                                                       &all_acl_entries));
  for (::p4::TableEntry* acl_table_entry : all_acl_entries) {
    RETURN_IF_ERROR(DeleteTableEntry(*acl_table_entry));
  }
  // Remove all the ACL tables from hardware & software.
  gtl::flat_hash_set<uint32> unique_physical_table_ids;
  for (uint32 acl_table_id : acl_table_ids) {
    ASSIGN_OR_RETURN(const AclTable* table,
                     bcm_table_manager_->GetReadOnlyAclTable(acl_table_id));
    unique_physical_table_ids.insert(table->PhysicalTableId());
    RETURN_IF_ERROR(bcm_table_manager_->DeleteTable(acl_table_id));
  }
  for (uint32 id : unique_physical_table_ids) {
    // Remove unique physical tables from the hardware.
    RETURN_IF_ERROR(bcm_sdk_interface_->DestroyAclTable(unit_, id));
  }
  return util::OkStatus();
}

::util::Status BcmAclManager::SplitAclControlBlocks(
    const P4ControlBlock& control_block,
    BcmAclStageMap<P4ControlBlock>* stage_blocks) const {
  for (const auto& statement : control_block.statements()) {
    // Find the table at the root of this statement.
    P4ControlTableRef table_reference;
    if (!GetStatementTableReference(statement, &table_reference)) {
      VLOG(1) << "Ignoring statement due to non-table root: "
              << statement.ShortDebugString() << ".";
      continue;
    }
    // Find the ACL stage this statement applies to (VFP, IFP, EFP).
    BcmAclStage stage =
        AclTable::P4PipelineToBcmAclStage(table_reference.pipeline_stage());
    if (stage != BCM_ACL_STAGE_UNKNOWN) {
      *(*stage_blocks)[stage].add_statements() = statement;
    }
  }
  return ::util::OkStatus();
}

::util::StatusOr<std::vector<BcmAclManager::PhysicalAclTable>>
BcmAclManager::PhysicalAclTablesFromPipeline(
    const P4ControlBlock& control_block) const {
  // Generate per-stage pipelines.
  BcmAclStageMap<P4ControlBlock> stage_blocks;
  RETURN_IF_ERROR(SplitAclControlBlocks(control_block, &stage_blocks));
  // Create pipelines for each stage.
  BcmAclStageMap<std::unique_ptr<BcmAclPipeline>> stage_pipelines;
  for (const auto& pair : stage_blocks) {
    BcmAclStage stage = pair.first;
    auto pipeline_status = BcmAclPipeline::CreateBcmAclPipeline(pair.second);
    RETURN_IF_ERROR_WITH_APPEND(pipeline_status.status())
        << " Failed while generating pipeline for stage "
        << BcmAclStage_Name(stage) << ".";
    stage_pipelines.insert(
        std::make_pair(stage, std::move(pipeline_status.ValueOrDie())));
  }
  // Insert per-stage pipelines.
  std::vector<PhysicalAclTable> physical_acl_tables;
  for (const auto& pair : stage_pipelines) {
    BcmAclStage stage = pair.first;
    const std::vector<BcmAclPipeline::PhysicalTableAsVector>& pipeline =
        pair.second->pipeline();
    // Create a physical table for each physical table in the pipeline.
    for (const auto& physical_table : pipeline) {
      auto result = GeneratePhysicalAclTables(stage, physical_table);
      RETURN_IF_ERROR(result.status());
      physical_acl_tables.emplace_back(std::move(result.ValueOrDie()));
    }
  }
  return physical_acl_tables;
}

::util::StatusOr<BcmAclManager::PhysicalAclTable>
BcmAclManager::GeneratePhysicalAclTables(
    BcmAclStage stage,
    const BcmAclPipeline::PhysicalTableAsVector& physical_table) const {
  // Create all the AclTables in the physical table.
  PhysicalAclTable physical_acl_table;
  physical_acl_table.stage = stage;
  for (const BcmAclPipelineTable& pipeline_table : physical_table) {
    uint32 table_id = pipeline_table.table.table_id();
    ::p4::config::Table p4_table;
    RETURN_IF_ERROR(p4_table_mapper_->LookupTable(table_id, &p4_table));
    physical_acl_table.logical_tables.emplace_back(p4_table, stage,
                                                   pipeline_table.priority);
  }
  return physical_acl_table;
}

::util::StatusOr<int> BcmAclManager::InstallPhysicalTable(
    const BcmAclManager::PhysicalAclTable& physical_acl_table) const {
  if (physical_acl_table.logical_tables.empty()) {
    return MAKE_ERROR(ERR_INTERNAL) << "We tried to create an empty physical "
                                       "table. This is likely a bug.";
  }
  // Get the field types.
  gtl::flat_hash_set<BcmField::Type, EnumHash<BcmField::Type>> bcm_fields;
  for (const AclTable& table : physical_acl_table.logical_tables) {
    ASSIGN_OR_RETURN(auto fields, GetTableMatchTypes(table));
    for (const BcmField::Type field : fields) {
      bcm_fields.insert(field);
    }
  }
  // Set up and install the BcmAclTable.
  BcmAclTable bcm_acl_table;
  // The first logical table always has the highest priority.
  bcm_acl_table.set_priority(physical_acl_table.logical_tables[0].Priority());
  for (const BcmField::Type& bcm_type : bcm_fields) {
    bcm_acl_table.add_fields()->set_type(bcm_type);
  }
  bcm_acl_table.set_stage(physical_acl_table.stage);
  auto install_result =
      bcm_sdk_interface_->CreateAclTable(unit_, bcm_acl_table);
  RETURN_IF_ERROR(install_result.status())
      << " Failed to install physical table in unit " << unit_
      << ". Table: " << bcm_acl_table.ShortDebugString() << ".";
  LOG(INFO) << "Successfully installed physical table on unit " << unit_
            << " as table " << install_result.ValueOrDie()
            << ". Table: " << bcm_acl_table.ShortDebugString() << ".";
  return install_result.ValueOrDie();
}

::util::StatusOr<gtl::flat_hash_set<BcmField::Type, EnumHash<BcmField::Type>>>
BcmAclManager::GetTableMatchTypes(const AclTable& table) const {
  gtl::flat_hash_set<BcmField::Type, EnumHash<BcmField::Type>> bcm_fields;
  for (uint32 field_id : table.MatchFields()) {
    MappedField field;
    RETURN_IF_ERROR_WITH_APPEND(
        p4_table_mapper_->MapMatchField(table.Id(), field_id, &field))
        << " Failed to get match types for table " << table.Id() << ".";
    BcmField::Type bcm_type =
        bcm_table_manager_->P4FieldTypeToBcmFieldType(field.type());
    if (bcm_type == BcmField::UNKNOWN) {
      // TODO: Once we get full capability, this should be changed to
      // return an INVALID_PARAM error.
      LOG(WARNING) << "Table " << table.Id()
                   << " contains unsupported match field: "
                   << P4FieldType_Name(field.type()) << " (" << field.type()
                   << ").";
    } else {
      bcm_fields.insert(bcm_type);
    }
  }
  return bcm_fields;
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
