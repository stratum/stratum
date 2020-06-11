// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_node.h"

#include <unistd.h>

#include <memory>

#include "absl/memory/memory.h"
#include "absl/strings/strip.h"
#include "archive.h"
#include "archive_entry.h"
#include "bf_rt/bf_rt_init.hpp"
#include "nlohmann/json.hpp"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/gtl/cleanup.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/error.pb.h"

// Not all SDE headers include "extern C".
extern "C" {
#include "dvm/bf_drv_profile.h"
#include "pkt_mgr/pkt_mgr_intf.h"
#include "tofino/bf_pal/dev_intf.h"
}

DEFINE_string(bfrt_sde_config_dir, "/var/run/stratum/bfrt_config",
              "The dir used by the SDE to load the device configuration.");

namespace stratum {
namespace hal {
namespace barefoot {
BfRtNode::~BfRtNode() = default;

::util::Status BfRtNode::PushChassisConfig(const ChassisConfig& config,
                                           uint64 node_id) {
  absl::WriterMutexLock l(&lock_);
  node_id_ = node_id;
  // RETURN_IF_ERROR(bfrt_table_manager_->PushChassisConfig(config, node_id));
  // RETURN_IF_ERROR(
  //     bfrt_action_profile_manager_->PushChassisConfig(config, node_id));
  RETURN_IF_ERROR(bfrt_packetio_manager_->PushChassisConfig(config, node_id));
  initialized_ = true;

  return ::util::OkStatus();
}

::util::Status BfRtNode::VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id) {
  // RETURN_IF_ERROR(bfrt_table_manager_->VerifyChassisConfig(config, node_id));
  // RETURN_IF_ERROR(
  //     bfrt_action_profile_manager_->VerifyChassisConfig(config, node_id));
  RETURN_IF_ERROR(bfrt_packetio_manager_->VerifyChassisConfig(config, node_id));
  return ::util::OkStatus();
}

::util::Status BfRtNode::PushForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  // SaveForwardingPipelineConfig + CommitForwardingPipelineConfig
  RETURN_IF_ERROR(SaveForwardingPipelineConfig(config));
  return CommitForwardingPipelineConfig();
}

::util::Status BfRtNode::SaveForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&lock_);
  RETURN_IF_ERROR(VerifyForwardingPipelineConfig(config));
  uint64 cookie = 0;
  if (config.has_cookie()) {
    cookie = config.cookie().cookie();
  }

  p4info_.CopyFrom(config.p4info());
  RETURN_IF_ERROR(LoadP4DeviceConfig(config.p4_device_config()));

  return ::util::OkStatus();
}

::util::Status BfRtNode::CommitForwardingPipelineConfig() {
  absl::WriterMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(initialized_) << "Not initialized";
  CHECK_RETURN_IF_FALSE(bfrt_config_.programs_size() > 0);
  CHECK_RETURN_IF_FALSE(bfrt_config_.device() >= 0);

  if (pipeline_initialized_) {
    // RETURN_IF_BFRT_ERROR(bf_device_remove(unit_));
  }

  BFRT_RETURN_IF_ERROR(bf_pal_device_warm_init_begin(
      unit_, BF_DEV_WARM_INIT_FAST_RECFG, BF_DEV_SERDES_UPD_NONE,
      /* upgrade_agents */ true));
  bf_device_profile_t device_profile = {};

  // Commit new files to disk and build device profile for SDE to load.
  RETURN_IF_ERROR(RecursivelyCreateDir(FLAGS_bfrt_sde_config_dir));
  // Need to extend the lifetime of the path strings until the SDE read them.
  std::vector<std::unique_ptr<std::string>> path_strings;
  device_profile.num_p4_programs = bfrt_config_.programs_size();
  for (int i = 0; i < bfrt_config_.programs_size(); ++i) {
    const auto& program = bfrt_config_.programs(i);
    const std::string program_path =
        absl::StrCat(FLAGS_bfrt_sde_config_dir, "/", program.name());
    auto bfrt_path = absl::make_unique<std::string>(
        absl::StrCat(program_path, "/bfrt.json"));
    RETURN_IF_ERROR(RecursivelyCreateDir(program_path));
    RETURN_IF_ERROR(WriteStringToFile(program.bfrt(), *bfrt_path));

    bf_p4_program_t* p4_program = &device_profile.p4_programs[i];
    ::snprintf(p4_program->prog_name, _PI_UPDATE_MAX_NAME_SIZE, "%s",
               program.name().c_str());
    p4_program->bfrt_json_file = &(*bfrt_path)[0];
    p4_program->num_p4_pipelines = program.pipelines_size();
    path_strings.emplace_back(std::move(bfrt_path));
    CHECK_RETURN_IF_FALSE(program.pipelines_size() > 0);
    for (int j = 0; j < program.pipelines_size(); ++j) {
      const auto& pipeline = program.pipelines(j);
      const std::string pipeline_path =
          absl::StrCat(program_path, "/", pipeline.name());
      auto context_path = absl::make_unique<std::string>(
          absl::StrCat(pipeline_path, "/context.json"));
      auto config_path = absl::make_unique<std::string>(
          absl::StrCat(pipeline_path, "/tofino.bin"));
      RETURN_IF_ERROR(RecursivelyCreateDir(pipeline_path));
      RETURN_IF_ERROR(WriteStringToFile(pipeline.context(), *context_path));
      RETURN_IF_ERROR(WriteStringToFile(pipeline.config(), *config_path));

      bf_p4_pipeline_t* pipeline_profile = &p4_program->p4_pipelines[j];
      ::snprintf(pipeline_profile->p4_pipeline_name, _PI_UPDATE_MAX_NAME_SIZE,
                 "%s", pipeline.name().c_str());
      pipeline_profile->cfg_file = &(*config_path)[0];
      pipeline_profile->runtime_context_file = &(*context_path)[0];
      path_strings.emplace_back(std::move(config_path));
      path_strings.emplace_back(std::move(context_path));

      CHECK_RETURN_IF_FALSE(pipeline.scope_size() <= MAX_P4_PIPELINES);
      pipeline_profile->num_pipes_in_scope = pipeline.scope_size();
      for (int p = 0; p < pipeline.scope_size(); ++p) {
        const auto& scope = pipeline.scope(p);
        pipeline_profile->pipe_scope[p] = scope;
      }
    }
  }

  // bf_device_add?
  // This call re-initializes most SDE components.
  BFRT_RETURN_IF_ERROR(bf_pal_device_add(unit_, &device_profile));
  BFRT_RETURN_IF_ERROR(bf_pal_device_warm_init_end(unit_));

  // Push pipeline config to the managers
  BFRT_RETURN_IF_ERROR(bfrt_device_manager_->bfRtInfoGet(
      unit_, bfrt_config_.programs(0).name(), &bfrt_info_));

  RETURN_IF_ERROR(bfrt_id_mapper_->PushPipelineInfo(p4info_, bfrt_info_));
  RETURN_IF_ERROR(
      bfrt_packetio_manager_->PushForwardingPipelineConfig(p4info_));
  // FIXME(Yi): We need to scan all context.json to build correct mapping for
  // ActionProfiles and ActionSelectors. We may remove this workaround in the
  // future.
  for (int i = 0; i < bfrt_config_.programs_size(); ++i) {
    const auto& program = bfrt_config_.programs(i);
    for (int j = 0; j < program.pipelines_size(); ++j) {
      const auto& pipeline = program.pipelines(j);
      RETURN_IF_ERROR(bfrt_id_mapper_->BuildActionProfileMapping(
          p4info_, bfrt_info_, pipeline.context()));
    }
  }

  RETURN_IF_ERROR(bfrt_table_manager_->PushPipelineInfo(p4info_, bfrt_info_));
  RETURN_IF_ERROR(
      bfrt_action_profile_manager_->PushPipelineInfo(p4info_, bfrt_info_));

  pipeline_initialized_ = true;
  return ::util::OkStatus();
}

::util::Status BfRtNode::VerifyForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  CHECK_RETURN_IF_FALSE(config.has_p4info()) << "Missing P4 info";
  CHECK_RETURN_IF_FALSE(!config.p4_device_config().empty())
      << "Missing P4 device config";
  return ::util::OkStatus();
}

::util::Status BfRtNode::Shutdown() {
  // RETURN_IF_BFRT_ERROR(bf_device_remove(unit_));
  return ::util::OkStatus();
}

::util::Status BfRtNode::Freeze() { return ::util::OkStatus(); }

::util::Status BfRtNode::Unfreeze() { return ::util::OkStatus(); }

::util::Status BfRtNode::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  absl::WriterMutexLock l(&lock_);
  bool success = true;
  auto session = bfrt::BfRtSession::sessionCreate();
  session->beginBatch();
  for (const auto& update : req.updates()) {
    ::util::Status status = ::util::OkStatus();
    switch (update.entity().entity_case()) {
      case ::p4::v1::Entity::kTableEntry:
        status = bfrt_table_manager_->WriteTableEntry(
            session, update.type(), update.entity().table_entry());
        break;
      case ::p4::v1::Entity::kExternEntry:
        status = WriteExternEntry(session, update.type(),
                                  update.entity().extern_entry());
        break;
      case ::p4::v1::Entity::kActionProfileMember:
        status = bfrt_action_profile_manager_->WriteActionProfileMember(
            session, update.type(), update.entity().action_profile_member());
        break;
      case ::p4::v1::Entity::kActionProfileGroup:
        status = bfrt_action_profile_manager_->WriteActionProfileGroup(
            session, update.type(), update.entity().action_profile_group());
        break;
      case ::p4::v1::Entity::kMeterEntry:
      case ::p4::v1::Entity::kDirectMeterEntry:
      case ::p4::v1::Entity::kCounterEntry:
      case ::p4::v1::Entity::kDirectCounterEntry:
      case ::p4::v1::Entity::kPacketReplicationEngineEntry:
      case ::p4::v1::Entity::kValueSetEntry:
      case ::p4::v1::Entity::kRegisterEntry:
      case ::p4::v1::Entity::kDigestEntry:
      default:
        status = MAKE_ERROR()
                 << "Unsupported entity type: " << update.ShortDebugString();
        break;
    }
    success &= status.ok();
    results->push_back(status);
  }
  session->endBatch(true);

  if (!success) {
    return MAKE_ERROR(ERR_AT_LEAST_ONE_OPER_FAILED)
           << "One or more write operations failed.";
  }

  LOG(INFO) << "P4-based forwarding entities written successfully to node with "
            << "ID " << node_id_ << ".";
  return ::util::OkStatus();
}

::util::Status BfRtNode::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  absl::WriterMutexLock l(&lock_);
  ::p4::v1::ReadResponse resp;
  bool success = true;
  auto session = bfrt::BfRtSession::sessionCreate();
  session->beginBatch();
  for (auto entity : req.entities()) {
    switch (entity.entity_case()) {
      case ::p4::v1::Entity::kTableEntry: {
        auto status =
            bfrt_table_manager_->ReadTableEntry(session, entity.table_entry());
        if (!status.ok()) {
          success = false;
          details->push_back(status.status());
          break;
        }
        auto table_entry = status.ValueOrDie();
        auto entity_resp = resp.add_entities();
        *entity_resp->mutable_table_entry() = table_entry;
        break;
      }
      case ::p4::v1::Entity::kExternEntry: {
        auto status = ReadExternEntry(session, entity.extern_entry());
        if (!status.ok()) {
          success = false;
          details->push_back(status.status());
          break;
        }
        auto extern_entry = status.ValueOrDie();
        auto entity_resp = resp.add_entities();
        *entity_resp->mutable_extern_entry() = extern_entry;
        break;
      }
      case ::p4::v1::Entity::kActionProfileMember: {
        auto status = bfrt_action_profile_manager_->ReadActionProfileMember(
            session, entity.action_profile_member());
        if (!status.ok()) {
          success = false;
          details->push_back(status.status());
          break;
        }
        auto action_profile_member = status.ValueOrDie();
        auto entity_resp = resp.add_entities();
        *entity_resp->mutable_action_profile_member() = action_profile_member;
        break;
      }

      case ::p4::v1::Entity::kActionProfileGroup: {
        auto status = bfrt_action_profile_manager_->ReadActionProfileGroup(
            session, entity.action_profile_group());
        if (!status.ok()) {
          success = false;
          details->push_back(status.status());
          break;
        }
        auto action_profile_group = status.ValueOrDie();
        auto entity_resp = resp.add_entities();
        *entity_resp->mutable_action_profile_group() = action_profile_group;
        break;
      }
      case ::p4::v1::Entity::kMeterEntry:
      case ::p4::v1::Entity::kDirectMeterEntry:
      case ::p4::v1::Entity::kCounterEntry:
      case ::p4::v1::Entity::kDirectCounterEntry:
      case ::p4::v1::Entity::kPacketReplicationEngineEntry:
      case ::p4::v1::Entity::kValueSetEntry:
      case ::p4::v1::Entity::kRegisterEntry:
      case ::p4::v1::Entity::kDigestEntry:
      default: {
        success = false;
        details->push_back(MAKE_ERROR(ERR_UNIMPLEMENTED)
                           << "Unsupported entity type: "
                           << entity.ShortDebugString());
        break;
      }
    }
  }
  session->endBatch(true);
  CHECK_RETURN_IF_FALSE(writer->Write(resp))
      << "Write to stream channel failed.";
  if (!success) {
    return MAKE_ERROR(ERR_AT_LEAST_ONE_OPER_FAILED)
           << "One or more write operations failed.";
  }
  return ::util::OkStatus();
}

::util::Status BfRtNode::RegisterPacketReceiveWriter(
    const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer) {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return bfrt_packetio_manager_->RegisterPacketReceiveWriter(writer);
}

::util::Status BfRtNode::UnregisterPacketReceiveWriter() {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return bfrt_packetio_manager_->UnregisterPacketReceiveWriter();
}

::util::Status BfRtNode::TransmitPacket(const ::p4::v1::PacketOut& packet) {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return bfrt_packetio_manager_->TransmitPacket(packet);
}

::util::Status BfRtNode::WriteExternEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type, const ::p4::v1::ExternEntry& entry) {
  switch (entry.extern_type_id()) {
    case kTnaExternActionProfileId:
    case kTnaExternActionSelectorId:
      return bfrt_action_profile_manager_->WriteActionProfileEntry(bfrt_session,
                                                                   type, entry);
      break;
    default:
      RETURN_ERROR() << "Unsupport extern entry " << entry.ShortDebugString();
  }
}

::util::StatusOr<::p4::v1::ExternEntry> BfRtNode::ReadExternEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::ExternEntry& entry) {
  switch (entry.extern_type_id()) {
    case kTnaExternActionProfileId:
    case kTnaExternActionSelectorId:
      return bfrt_action_profile_manager_->ReadActionProfileEntry(bfrt_session,
                                                                  entry);
      break;
    default:
      RETURN_ERROR() << "Unsupport extern entry " << entry.ShortDebugString();
  }
}

namespace {
// Helper functions to extract the contents of first file named filename from
// an in-memory archive.
::util::StatusOr<std::string> ExtractFromArchive(const std::string& archive,
                                                 const std::string& filename) {
  struct archive* a = archive_read_new();
  archive_read_support_filter_bzip2(a);
  archive_read_support_filter_xz(a);
  archive_read_support_format_tar(a);
  int r = archive_read_open_memory(a, archive.c_str(), archive.size());
  CHECK_RETURN_IF_FALSE(r == ARCHIVE_OK) << "Failed to read archive";
  auto cleanup = gtl::MakeCleanup([&a]() { archive_read_free(a); });
  struct archive_entry* entry;
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    std::string path_name = archive_entry_pathname(entry);
    if (absl::StripSuffix(path_name, filename) != path_name) {
      VLOG(2) << "Found file: " << path_name;
      std::string content;
      content.resize(archive_entry_size(entry));
      CHECK_RETURN_IF_FALSE(archive_read_data(a, &content[0], content.size()) ==
                            content.size());
      return content;
    }
  }
  return MAKE_ERROR(ERR_ENTRY_NOT_FOUND) << "File not found: " << filename;
}
}  // namespace

::util::Status BfRtNode::LoadP4DeviceConfig(
    const std::string& p4_device_config) {
  // Try a parse of BfrtDeviceConfig.
  {
    BfrtDeviceConfig config;
    if (config.ParseFromString(p4_device_config)) {
      bfrt_config_ = config;
      return ::util::OkStatus();
    }
  }

  // Find <prog_name>.conf file
  nlohmann::json conf;
  {
    ASSIGN_OR_RETURN(auto conf_content,
                     ExtractFromArchive(p4_device_config, ".conf"));
    try {
      conf = nlohmann::json::parse(conf_content);
      LOG(INFO) << conf.dump();
    } catch (nlohmann::json::exception& e) {
      return MAKE_ERROR(ERR_INTERNAL) << "Failed to parse .conf: " << e.what();
    }
  }

  // Translate JSON conf to protobuf.
  try {
    CHECK_RETURN_IF_FALSE(conf["p4_devices"].size() == 1)
        << "Stratum only supports single devices.";
    auto device = conf["p4_devices"][0];  // Only support single devices for now
    bfrt_config_.set_device(device["device-id"]);
    for (const auto& program : device["p4_programs"]) {
      auto p = bfrt_config_.add_programs();
      p->set_name(program["program-name"]);
      ASSIGN_OR_RETURN(auto bfrt_content,
                       ExtractFromArchive(p4_device_config, "bfrt.json"));
      p->set_bfrt(bfrt_content);
      for (const auto& pipeline : program["p4_pipelines"]) {
        auto pipe = p->add_pipelines();
        pipe->set_name(pipeline["p4_pipeline_name"]);
        for (const auto& scope : pipeline["pipe_scope"]) {
          pipe->add_scope(scope);
        }
        ASSIGN_OR_RETURN(
            auto context_content,
            ExtractFromArchive(p4_device_config,
                               absl::StrCat(pipe->name(), "/context.json")));
        pipe->set_context(context_content);
        ASSIGN_OR_RETURN(
            auto config_content,
            ExtractFromArchive(p4_device_config,
                               absl::StrCat(pipe->name(), "/tofino.bin")));
        pipe->set_config(config_content);
      }
    }
  } catch (nlohmann::json::exception& e) {
    return MAKE_ERROR(ERR_INTERNAL) << e.what();
  }

  VLOG(2) << bfrt_config_.DebugString();
  return ::util::OkStatus();
}

// Factory function for creating the instance of the class.
std::unique_ptr<BfRtNode> BfRtNode::CreateInstance(
    BfRtTableManager* bfrt_table_manager,
    BfRtActionProfileManager* bfrt_action_profile_manager,
    BfrtPacketioManager* bfrt_packetio_manager,
    ::bfrt::BfRtDevMgr* bfrt_device_manager, BfRtIdMapper* bfrt_id_mapper,
    int unit) {
  return absl::WrapUnique(new BfRtNode(
      bfrt_table_manager, bfrt_action_profile_manager, bfrt_packetio_manager,
      bfrt_device_manager, bfrt_id_mapper, unit));
}

BfRtNode::BfRtNode(BfRtTableManager* bfrt_table_manager,
                   BfRtActionProfileManager* bfrt_action_profile_manager,
                   BfrtPacketioManager* bfrt_packetio_manager,
                   ::bfrt::BfRtDevMgr* bfrt_device_manager,
                   BfRtIdMapper* bfrt_id_mapper, int unit)
    : pipeline_initialized_(false),
      initialized_(false),
      bfrt_table_manager_(ABSL_DIE_IF_NULL(bfrt_table_manager)),
      bfrt_action_profile_manager_(
          ABSL_DIE_IF_NULL(bfrt_action_profile_manager)),
      bfrt_packetio_manager_(bfrt_packetio_manager),
      bfrt_device_manager_(ABSL_DIE_IF_NULL(bfrt_device_manager)),
      bfrt_id_mapper_(ABSL_DIE_IF_NULL(bfrt_id_mapper)),
      node_id_(0),
      unit_(unit) {}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
