// Copyright 2020-present Open Networking Foundation
// Copyright 2020 Intel/Barefoot Networks
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_node.h"

#include <unistd.h>

#include <memory>

#include "absl/memory/memory.h"
#include "absl/strings/strip.h"
#include "archive.h"
#include "archive_entry.h"
#include "bf_rt/bf_rt_init.hpp"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/gtl/cleanup.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/error.pb.h"

extern "C" {
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
  (void)config;
  absl::WriterMutexLock l(&lock_);
  node_id_ = node_id;
  initialized_ = true;

  return ::util::OkStatus();
}

::util::Status BfRtNode::VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id) {
  (void)config;
  (void)node_id;
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
  if (!initialized_) {
    RETURN_ERROR() << "Not initialized";
  }
  BFRT_RETURN_IF_ERROR(bf_pal_device_warm_init_begin(
      unit_, BF_DEV_WARM_INIT_FAST_RECFG, BF_DEV_SERDES_UPD_NONE,
      /* upgrade_agents */ true));
  bf_device_profile_t device_profile = {};

  // Commit new files to disk for SDE to load.
  std::string context_path =
      absl::StrCat(FLAGS_bfrt_sde_config_dir, "/context.json");
  std::string bfrt_path = absl::StrCat(FLAGS_bfrt_sde_config_dir, "/bfrt.json");
  std::string tofino_bin_path =
      absl::StrCat(FLAGS_bfrt_sde_config_dir, "/tofino.bin");
  RETURN_IF_ERROR(RecursivelyCreateDir(FLAGS_bfrt_sde_config_dir));
  RETURN_IF_ERROR(WriteStringToFile(ctx_json_, context_path));
  RETURN_IF_ERROR(WriteStringToFile(bfrt_file_, bfrt_path));
  RETURN_IF_ERROR(WriteStringToFile(tofino_bin_, tofino_bin_path));
  auto cleanup =
      gtl::MakeCleanup([&context_path, &bfrt_path, &tofino_bin_path]() {
        RemoveFile(context_path);
        RemoveFile(bfrt_path);
        RemoveFile(tofino_bin_path);
      });
  ctx_json_.clear();
  bfrt_file_.clear();
  tofino_bin_.clear();

  // TODO(Yi): Now we only support single P4 program
  device_profile.num_p4_programs = 1;
  bf_p4_program_t* p4_program = &device_profile.p4_programs[0];
  strncpy(p4_program->prog_name, prog_name_.c_str(), _PI_UPDATE_MAX_NAME_SIZE);
  p4_program->bfrt_json_file = &bfrt_path[0];
  p4_program->num_p4_pipelines = 1;

  // TODO(Yi): Now we applies single pipelines to all HW pipeline
  bf_p4_pipeline_t* pipeline_profile = &p4_program->p4_pipelines[0];
  ::snprintf(pipeline_profile->p4_pipeline_name, _PI_UPDATE_MAX_NAME_SIZE, "%s",
             "pipe");
  pipeline_profile->cfg_file = &tofino_bin_path[0];
  pipeline_profile->runtime_context_file = &context_path[0];

  // FIXME(YI): do we need to put p4info here?
  pipeline_profile->pi_config_file = nullptr;

  // Single P4 pipeline for all HW pipeline
  pipeline_profile->num_pipes_in_scope = 4;
  // pipe_scope = [0, 1, 2, 3]
  for (int p = 0; p < 4; ++p) {
    pipeline_profile->pipe_scope[p] = p;
  }

  BFRT_RETURN_IF_ERROR(bf_pal_device_add(unit_, &device_profile));
  BFRT_RETURN_IF_ERROR(bf_pal_device_warm_init_end(unit_));

  // Push pipeline config to the managers
  BFRT_RETURN_IF_ERROR(
      bfrt_device_manager_->bfRtInfoGet(unit_, prog_name_, &bfrt_info_));
  RETURN_IF_ERROR(bfrt_id_mapper_->PushPipelineInfo(p4info_, bfrt_info_));
  RETURN_IF_ERROR(bfrt_table_manager_->PushPipelineInfo(p4info_, bfrt_info_));

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

::util::Status BfRtNode::Shutdown() { return ::util::OkStatus(); }

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
      case ::p4::v1::Entity::kActionProfileMember:
      case ::p4::v1::Entity::kActionProfileGroup:
      case ::p4::v1::Entity::kMeterEntry:
      case ::p4::v1::Entity::kDirectMeterEntry:
      case ::p4::v1::Entity::kCounterEntry:
      case ::p4::v1::Entity::kDirectCounterEntry:
      case ::p4::v1::Entity::kPacketReplicationEngineEntry:
      case ::p4::v1::Entity::kValueSetEntry:
      case ::p4::v1::Entity::kRegisterEntry:
      case ::p4::v1::Entity::kDigestEntry:
      default:
        results->push_back(MAKE_ERROR() << "Unsupported entity type: "
                                        << update.ShortDebugString());
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
  return ::util::OkStatus();
}

::util::Status BfRtNode::RegisterPacketReceiveWriter(
    const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer) {
  return ::util::OkStatus();
}

::util::Status BfRtNode::UnregisterPacketReceiveWriter() {
  return ::util::OkStatus();
}

::util::Status BfRtNode::TransmitPacket(const ::p4::v1::PacketOut& packet) {
  return ::util::OkStatus();
}

::util::Status BfRtNode::LoadP4DeviceConfig(
    const std::string& p4_device_config) {
  struct archive* a = archive_read_new();
  archive_read_support_filter_bzip2(a);
  archive_read_support_format_tar(a);
  int r = archive_read_open_memory(a, p4_device_config.c_str(),
                                   p4_device_config.size());
  CHECK_RETURN_IF_FALSE(r == ARCHIVE_OK) << "Failed to read archive";
  auto cleanup = gtl::MakeCleanup([&a]() { archive_read_free(a); });

  // First entry should be a directory named <prog_name>.
  {
    struct archive_entry* entry;
    CHECK_RETURN_IF_FALSE(archive_read_next_header(a, &entry) == ARCHIVE_OK)
        << "Could not read archive header.";
    CHECK_RETURN_IF_FALSE(archive_entry_filetype(entry) == AE_IFDIR)
        << "Expected a directory at root.";
    prog_name_ =
        std::string(absl::StripSuffix(archive_entry_pathname(entry), "/"));
  }

  // Read the remaining parameters
  std::string p4info;
  struct archive_entry* entry;
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    std::string path_name = archive_entry_pathname(entry);

    if (absl::StripSuffix(path_name, "p4info.txt") != path_name) {
      p4info.resize(archive_entry_size(entry));
      CHECK_RETURN_IF_FALSE(archive_read_data(a, &p4info[0], p4info.size()) >
                            0);
      VLOG(2) << "Found p4info file: " << path_name;
    }

    if (absl::StripSuffix(path_name, "bfrt.json") != path_name) {
      bfrt_file_.resize(archive_entry_size(entry));
      CHECK_RETURN_IF_FALSE(
          archive_read_data(a, &bfrt_file_[0], bfrt_file_.size()) > 0);
      VLOG(2) << "Found bfrt file: " << path_name;
    }

    if (absl::StripSuffix(path_name, "context.json") != path_name) {
      ctx_json_.resize(archive_entry_size(entry));
      CHECK_RETURN_IF_FALSE(
          archive_read_data(a, &ctx_json_[0], ctx_json_.size()) > 0);
      VLOG(2) << "Found context file: " << path_name;
    }

    if (absl::StripSuffix(path_name, "tofino.bin") != path_name) {
      tofino_bin_.resize(archive_entry_size(entry));
      CHECK_RETURN_IF_FALSE(
          archive_read_data(a, &tofino_bin_[0], tofino_bin_.size()) > 0);
      VLOG(2) << "Found tofino.bin file: " << path_name;
    }
  }

  CHECK_RETURN_IF_FALSE(!p4info.empty()) << "Could not find p4info file.";
  CHECK_RETURN_IF_FALSE(!bfrt_file_.empty()) << "Could not find bfrt file.";
  CHECK_RETURN_IF_FALSE(!ctx_json_.empty()) << "Could not find context file.";
  CHECK_RETURN_IF_FALSE(!tofino_bin_.empty())
      << "Could not find tofino.bin file.";

  return ::util::OkStatus();
}

// Factory function for creating the instance of the class.
std::unique_ptr<BfRtNode> BfRtNode::CreateInstance(
    BfRtTableManager* bfrt_table_manager,
    ::bfrt::BfRtDevMgr* bfrt_device_manager, BfRtIdMapper* bfrt_id_mapper,
    int unit) {
  return absl::WrapUnique(new BfRtNode(bfrt_table_manager, bfrt_device_manager,
                                       bfrt_id_mapper, unit));
}

BfRtNode::BfRtNode(BfRtTableManager* bfrt_table_manager,
                   ::bfrt::BfRtDevMgr* bfrt_device_manager,
                   BfRtIdMapper* bfrt_id_mapper, int unit)
    : pipeline_initialized_(false),
      initialized_(false),
      bfrt_table_manager_(ABSL_DIE_IF_NULL(bfrt_table_manager)),
      bfrt_device_manager_(ABSL_DIE_IF_NULL(bfrt_device_manager)),
      bfrt_id_mapper_(ABSL_DIE_IF_NULL(bfrt_id_mapper)),
      node_id_(0),
      unit_(unit) {}

void BfRtNode::SendPacketIn(const ::p4::v1::PacketIn& packet) {
  // acquire the lock during the Write: SendPacketIn may be called from
  // different threads and Write is not thread-safe.
  absl::MutexLock l(&rx_writer_lock_);
  if (rx_writer_ == nullptr) return;
  rx_writer_->Write(packet);
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
