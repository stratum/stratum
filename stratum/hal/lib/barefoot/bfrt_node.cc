// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_node.h"

#include <unistd.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/strip.h"
#include "bf_rt/bf_rt_init.hpp"
#include "libarchive/archive.h"
#include "libarchive/archive_entry.h"
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
#include "tofino/bf_pal/dev_intf.h"
}

DEFINE_string(bfrt_sde_config_dir, "/var/run/stratum/bfrt_config",
              "The dir used by the SDE to load the device configuration.");

namespace stratum {
namespace {
// Helper function to extract the contents of first file named filename from
// an in-memory archive.
::util::StatusOr<std::string> ExtractFromArchive(const std::string& archive,
                                                 const std::string& filename) {
  struct archive* a = archive_read_new();
  auto cleanup = gtl::MakeCleanup([&a]() { archive_read_free(a); });
  archive_read_support_filter_bzip2(a);
  archive_read_support_filter_xz(a);
  archive_read_support_format_tar(a);
  int r = archive_read_open_memory(a, archive.c_str(), archive.size());
  CHECK_RETURN_IF_FALSE(r == ARCHIVE_OK) << "Failed to read archive";
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

namespace hal {
namespace barefoot {
BfrtNode::~BfrtNode() = default;

::util::Status BfrtNode::PushChassisConfig(const ChassisConfig& config,
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

::util::Status BfrtNode::VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id) {
  // RETURN_IF_ERROR(bfrt_table_manager_->VerifyChassisConfig(config, node_id));
  // RETURN_IF_ERROR(
  //     bfrt_action_profile_manager_->VerifyChassisConfig(config, node_id));
  RETURN_IF_ERROR(bfrt_packetio_manager_->VerifyChassisConfig(config, node_id));
  return ::util::OkStatus();
}

::util::Status BfrtNode::PushForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  // SaveForwardingPipelineConfig + CommitForwardingPipelineConfig
  RETURN_IF_ERROR(SaveForwardingPipelineConfig(config));
  return CommitForwardingPipelineConfig();
}

::util::Status BfrtNode::SaveForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  absl::WriterMutexLock l(&lock_);
  RETURN_IF_ERROR(VerifyForwardingPipelineConfig(config));
  uint64 cookie = 0;
  if (config.has_cookie()) {
    cookie = config.cookie().cookie();
  }

  // Try parsing as BfPipelineConfig.
  {
    BfPipelineConfig pipeline_config;
    // The pipeline config is stored as raw bytes in the p4_device_config.
    if (pipeline_config.ParseFromString(config.p4_device_config())) {
      VLOG(1) << "Pipeline is in BfPipelineConfig format.";
      BfrtDeviceConfig bfrt_config;
      auto program = bfrt_config.add_programs();
      program->set_name(pipeline_config.p4_name());
      program->set_bfrt(pipeline_config.bfruntime_info());
      *program->mutable_p4info() = config.p4info();
      for (const auto& profile : pipeline_config.profiles()) {
        auto pipeline = program->add_pipelines();
        pipeline->set_name(profile.profile_name());
        pipeline->set_context(profile.context());
        pipeline->set_config(profile.binary());
        *pipeline->mutable_scope() = profile.pipe_scope();
      }
      bfrt_config_ = bfrt_config;
      VLOG(2) << bfrt_config_.DebugString();
      return ::util::OkStatus();
    }
  }

  // Find <prog_name>.conf file
  nlohmann::json conf;
  {
    ASSIGN_OR_RETURN(auto conf_content,
                     ExtractFromArchive(config.p4_device_config(), ".conf"));
    conf = nlohmann::json::parse(conf_content, nullptr, false);
    CHECK_RETURN_IF_FALSE(!conf.is_discarded()) << "Failed to parse .conf";
    VLOG(1) << ".conf content: " << conf.dump();
  }

  // Translate JSON conf to protobuf.
  try {
    BfrtDeviceConfig bfrt_config;
    CHECK_RETURN_IF_FALSE(conf["p4_devices"].size() == 1)
        << "Stratum only supports single devices.";
    // Only support single devices for now
    const auto& device = conf["p4_devices"][0];
    for (const auto& program : device["p4_programs"]) {
      auto p = bfrt_config.add_programs();
      p->set_name(program["program-name"]);
      ASSIGN_OR_RETURN(
          auto bfrt_content,
          ExtractFromArchive(config.p4_device_config(), "bfrt.json"));
      p->set_bfrt(bfrt_content);
      *p->mutable_p4info() = config.p4info();
      for (const auto& pipeline : program["p4_pipelines"]) {
        auto pipe = p->add_pipelines();
        pipe->set_name(pipeline["p4_pipeline_name"]);
        for (const auto& scope : pipeline["pipe_scope"]) {
          pipe->add_scope(scope);
        }
        ASSIGN_OR_RETURN(
            const auto context_content,
            ExtractFromArchive(config.p4_device_config(),
                               absl::StrCat(pipe->name(), "/context.json")));
        pipe->set_context(context_content);
        ASSIGN_OR_RETURN(
            const auto config_content,
            ExtractFromArchive(config.p4_device_config(),
                               absl::StrCat(pipe->name(), "/tofino.bin")));
        pipe->set_config(config_content);
      }
    }
    bfrt_config_ = bfrt_config;
  } catch (nlohmann::json::exception& e) {
    return MAKE_ERROR(ERR_INTERNAL) << e.what();
  }

  VLOG(2) << bfrt_config_.DebugString();

  return ::util::OkStatus();
}

::util::Status BfrtNode::CommitForwardingPipelineConfig() {
  absl::WriterMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(initialized_) << "Not initialized";
  CHECK_RETURN_IF_FALSE(bfrt_config_.programs_size() > 0);

  if (pipeline_initialized_) {
    // RETURN_IF_BFRT_ERROR(bf_device_remove(device_id_));
  }

  RETURN_IF_BFRT_ERROR(bf_pal_device_warm_init_begin(
      device_id_, BF_DEV_WARM_INIT_FAST_RECFG, BF_DEV_SERDES_UPD_NONE,
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
  RETURN_IF_BFRT_ERROR(bf_pal_device_add(device_id_, &device_profile));
  RETURN_IF_BFRT_ERROR(bf_pal_device_warm_init_end(device_id_));

  // Set SDE log levels for modules of interest.
  CHECK_RETURN_IF_FALSE(
      bf_sys_log_level_set(BF_MOD_BFRT, BF_LOG_DEST_STDOUT, BF_LOG_WARN) == 0);
  CHECK_RETURN_IF_FALSE(
      bf_sys_log_level_set(BF_MOD_PKT, BF_LOG_DEST_STDOUT, BF_LOG_WARN) == 0);

  RETURN_IF_BFRT_ERROR(bfrt_device_manager_->bfRtInfoGet(
      device_id_, bfrt_config_.programs(0).name(), &bfrt_info_));

  // Push pipeline config to the managers.
  // TODO(max): Do not pass bfrt_info_ (or other bfrt resources) directly to
  // other managers. On (a second) pipeline push these will get freed while
  // still in use.
  RETURN_IF_ERROR(
      bfrt_id_mapper_->PushForwardingPipelineConfig(bfrt_config_, bfrt_info_));
  RETURN_IF_ERROR(
      bfrt_packetio_manager_->PushForwardingPipelineConfig(bfrt_config_));
  RETURN_IF_ERROR(bfrt_table_manager_->PushForwardingPipelineConfig(
      bfrt_config_, bfrt_info_));
  RETURN_IF_ERROR(bfrt_action_profile_manager_->PushForwardingPipelineConfig(
      bfrt_config_, bfrt_info_));
  RETURN_IF_ERROR(bfrt_pre_manager_->PushForwardingPipelineConfig(bfrt_config_,
                                                                  bfrt_info_));
  RETURN_IF_ERROR(bfrt_counter_manager_->PushForwardingPipelineConfig(
      bfrt_config_, bfrt_info_));

  pipeline_initialized_ = true;
  return ::util::OkStatus();
}

::util::Status BfrtNode::VerifyForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) const {
  CHECK_RETURN_IF_FALSE(config.has_p4info()) << "Missing P4 info";
  CHECK_RETURN_IF_FALSE(!config.p4_device_config().empty())
      << "Missing P4 device config";
  RETURN_IF_ERROR(bfrt_table_manager_->VerifyForwardingPipelineConfig(config));
  return ::util::OkStatus();
}

::util::Status BfrtNode::Shutdown() {
  // RETURN_IF_BFRT_ERROR(bf_device_remove(device_id_));
  return ::util::OkStatus();
}

::util::Status BfrtNode::Freeze() { return ::util::OkStatus(); }

::util::Status BfrtNode::Unfreeze() { return ::util::OkStatus(); }

::util::Status BfrtNode::WriteForwardingEntries(
    const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results) {
  absl::WriterMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(req.device_id() == node_id_)
      << "Request device id must be same as id of this BfrtNode.";
  CHECK_RETURN_IF_FALSE(req.atomicity() ==
                        ::p4::v1::WriteRequest::CONTINUE_ON_ERROR)
      << "Request atomicity "
      << ::p4::v1::WriteRequest::Atomicity_Name(req.atomicity())
      << " is not supported.";

  bool success = true;
  auto session = bfrt::BfRtSession::sessionCreate();
  CHECK_RETURN_IF_FALSE(session != nullptr) << "Unable to create session.";
  RETURN_IF_BFRT_ERROR(session->beginBatch());
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
      case ::p4::v1::Entity::kPacketReplicationEngineEntry:
        status = bfrt_pre_manager_->WritePreEntry(
            session, update.type(),
            update.entity().packet_replication_engine_entry());
        break;
      case ::p4::v1::Entity::kDirectCounterEntry:
        status = bfrt_table_manager_->WriteDirectCounterEntry(
            session, update.type(), update.entity().direct_counter_entry());
        break;
      case ::p4::v1::Entity::kCounterEntry:
        status = bfrt_counter_manager_->WriteIndirectCounterEntry(
            session, update.type(), update.entity().counter_entry());
        break;
      case ::p4::v1::Entity::kRegisterEntry: {
        status = bfrt_table_manager_->WriteRegisterEntry(
            session, update.type(), update.entity().register_entry());
        break;
      }
      case ::p4::v1::Entity::kMeterEntry:
      case ::p4::v1::Entity::kDirectMeterEntry:
      case ::p4::v1::Entity::kValueSetEntry:
      case ::p4::v1::Entity::kDigestEntry:
      default:
        status = MAKE_ERROR()
                 << "Unsupported entity type: " << update.ShortDebugString();
        break;
    }
    success &= status.ok();
    results->push_back(status);
  }
  RETURN_IF_BFRT_ERROR(session->endBatch(true));

  if (!success) {
    return MAKE_ERROR(ERR_AT_LEAST_ONE_OPER_FAILED)
           << "One or more write operations failed.";
  }

  LOG(INFO) << "P4-based forwarding entities written successfully to node with "
            << "ID " << node_id_ << ".";
  return ::util::OkStatus();
}

::util::Status BfrtNode::ReadForwardingEntries(
    const ::p4::v1::ReadRequest& req,
    WriterInterface<::p4::v1::ReadResponse>* writer,
    std::vector<::util::Status>* details) {
  absl::WriterMutexLock l(&lock_);
  CHECK_RETURN_IF_FALSE(req.device_id() == node_id_)
      << "Request device id must be same as id of this BfrtNode.";
  ::p4::v1::ReadResponse resp;
  bool success = true;
  auto session = bfrt::BfRtSession::sessionCreate();
  CHECK_RETURN_IF_FALSE(session != nullptr) << "Unable to create session.";
  for (const auto& entity : req.entities()) {
    switch (entity.entity_case()) {
      case ::p4::v1::Entity::kTableEntry: {
        auto status = bfrt_table_manager_->ReadTableEntry(
            session, entity.table_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kExternEntry: {
        auto status = ReadExternEntry(session, entity.extern_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kActionProfileMember: {
        auto status = bfrt_action_profile_manager_->ReadActionProfileMember(
            session, entity.action_profile_member(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kActionProfileGroup: {
        auto status = bfrt_action_profile_manager_->ReadActionProfileGroup(
            session, entity.action_profile_group(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kPacketReplicationEngineEntry: {
        auto status = bfrt_pre_manager_->ReadPreEntry(
            session, entity.packet_replication_engine_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kDirectCounterEntry: {
        auto status = bfrt_table_manager_->ReadDirectCounterEntry(
            session, entity.direct_counter_entry());
        if (!status.ok()) {
          success = false;
          details->push_back(status.status());
          break;
        }
        resp.add_entities()->mutable_direct_counter_entry()->CopyFrom(
            status.ValueOrDie());
        break;
      }
      case ::p4::v1::Entity::kCounterEntry: {
        auto status = bfrt_counter_manager_->ReadIndirectCounterEntry(
            session, entity.counter_entry());
        if (!status.ok()) {
          success = false;
          details->push_back(status.status());
          break;
        }
        resp.add_entities()->mutable_counter_entry()->CopyFrom(
            status.ValueOrDie());
        break;
      }
      case ::p4::v1::Entity::kRegisterEntry: {
        auto status = bfrt_table_manager_->ReadRegisterEntry(
            session, entity.register_entry(), writer);
        success &= status.ok();
        details->push_back(status);
        break;
      }
      case ::p4::v1::Entity::kMeterEntry:
      case ::p4::v1::Entity::kDirectMeterEntry:
      case ::p4::v1::Entity::kValueSetEntry:
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
  CHECK_RETURN_IF_FALSE(writer->Write(resp))
      << "Write to stream channel failed.";
  if (!success) {
    return MAKE_ERROR(ERR_AT_LEAST_ONE_OPER_FAILED)
           << "One or more read operations failed.";
  }
  return ::util::OkStatus();
}

::util::Status BfrtNode::RegisterPacketReceiveWriter(
    const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer) {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return bfrt_packetio_manager_->RegisterPacketReceiveWriter(writer);
}

::util::Status BfrtNode::UnregisterPacketReceiveWriter() {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return bfrt_packetio_manager_->UnregisterPacketReceiveWriter();
}

::util::Status BfrtNode::TransmitPacket(const ::p4::v1::PacketOut& packet) {
  absl::WriterMutexLock l(&lock_);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  return bfrt_packetio_manager_->TransmitPacket(packet);
}

::util::Status BfrtNode::WriteExternEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::Update::Type type, const ::p4::v1::ExternEntry& entry) {
  switch (entry.extern_type_id()) {
    case kTnaExternActionProfileId:
    case kTnaExternActionSelectorId:
      return bfrt_action_profile_manager_->WriteActionProfileEntry(bfrt_session,
                                                                   type, entry);
      break;
    default:
      RETURN_ERROR() << "Unsupported extern entry: " << entry.ShortDebugString()
                     << ".";
  }
}

::util::Status BfrtNode::ReadExternEntry(
    std::shared_ptr<bfrt::BfRtSession> bfrt_session,
    const ::p4::v1::ExternEntry& entry,
    WriterInterface<::p4::v1::ReadResponse>* writer) {
  switch (entry.extern_type_id()) {
    case kTnaExternActionProfileId:
    case kTnaExternActionSelectorId:
      return bfrt_action_profile_manager_->ReadActionProfileEntry(
          bfrt_session, entry, writer);
      break;
    default:
      RETURN_ERROR(ERR_OPER_NOT_SUPPORTED)
          << "Unsupported extern entry: " << entry.ShortDebugString() << ".";
  }
}

// Factory function for creating the instance of the class.
std::unique_ptr<BfrtNode> BfrtNode::CreateInstance(
    BfrtTableManager* bfrt_table_manager,
    BfrtActionProfileManager* bfrt_action_profile_manager,
    BfrtPacketioManager* bfrt_packetio_manager,
    BfrtPreManager* bfrt_pre_manager, BfrtCounterManager* bfrt_counter_manager,
    ::bfrt::BfRtDevMgr* bfrt_device_manager, BfrtIdMapper* bfrt_id_mapper,
    int device_id) {
  return absl::WrapUnique(new BfrtNode(
      bfrt_table_manager, bfrt_action_profile_manager, bfrt_packetio_manager,
      bfrt_pre_manager, bfrt_counter_manager, bfrt_device_manager,
      bfrt_id_mapper, device_id));
}

BfrtNode::BfrtNode(BfrtTableManager* bfrt_table_manager,
                   BfrtActionProfileManager* bfrt_action_profile_manager,
                   BfrtPacketioManager* bfrt_packetio_manager,
                   BfrtPreManager* bfrt_pre_manager,
                   BfrtCounterManager* bfrt_counter_manager,
                   ::bfrt::BfRtDevMgr* bfrt_device_manager,
                   BfrtIdMapper* bfrt_id_mapper, int device_id)
    : pipeline_initialized_(false),
      initialized_(false),
      bfrt_table_manager_(ABSL_DIE_IF_NULL(bfrt_table_manager)),
      bfrt_action_profile_manager_(
          ABSL_DIE_IF_NULL(bfrt_action_profile_manager)),
      bfrt_packetio_manager_(bfrt_packetio_manager),
      bfrt_pre_manager_(ABSL_DIE_IF_NULL(bfrt_pre_manager)),
      bfrt_counter_manager_(ABSL_DIE_IF_NULL(bfrt_counter_manager)),
      bfrt_device_manager_(ABSL_DIE_IF_NULL(bfrt_device_manager)),
      bfrt_id_mapper_(ABSL_DIE_IF_NULL(bfrt_id_mapper)),
      node_id_(0),
      device_id_(device_id) {}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
