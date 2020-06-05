// Copyright 2020-present Open Networking Foundation
// Copyright 2020 Intel/Barefoot Networks
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_node.h"

#include <unistd.h>

#include <memory>

#include "absl/memory/memory.h"
#include "bf_rt/bf_rt_init.hpp"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/lib/macros.h"
#include "stratum/public/proto/error.pb.h"

extern "C" {
#include "tofino/bf_pal/dev_intf.h"
}

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
  const char* p4_device_config = config.p4_device_config().c_str();

  // Structure of P4 device config for Barefoot device
  // {
  //   prog_name_size: uint32
  //   prog_name: byte[prog_name__size]
  //   tofino_bin_size: uint32
  //   tofino_bin: byte[tofino_bin_size]
  //   context_json_size: uint32
  //   context_json: byte[context_json_size]
  //   bfrt_json_size: uint32
  //   bf_rt_json: byte[bfrt_json_size]
  // }

  // From BF-PI implementation
  const char* device_data_curr = p4_device_config;
  uint32_t chunk_size;
  memcpy(&chunk_size, device_data_curr, sizeof(uint32_t));
  device_data_curr += sizeof(uint32_t);
  if (chunk_size > _PI_UPDATE_MAX_NAME_SIZE || chunk_size == 0) {
    RETURN_ERROR(ERR_OUT_OF_RANGE)
        << "invalid program name size " << chunk_size;
  }
  strncpy(prog_name_, device_data_curr, chunk_size);
  prog_name_[chunk_size] = '\0';
  device_data_curr += chunk_size;

  // TODO(YI): lots of duplicated code, move to a function
  // Tofino bin
  memcpy(&chunk_size, device_data_curr, sizeof(uint32_t));
  device_data_curr += sizeof(uint32_t);
  snprintf(tofino_bin_path_, _PI_UPDATE_MAX_TMP_FILENAME_SIZE,
           "%s-tofino.bin.XXXXXX", prog_name_);
  int cfg_fd = mkstemp(tofino_bin_path_);
  if (cfg_fd == -1) {
    RETURN_ERROR() << "error when trying to create temp bin file "
                   << tofino_bin_path_;
  }
  ssize_t written = 0;
  while ((written = write(cfg_fd, device_data_curr, chunk_size)) != -1) {
    device_data_curr += written;
    chunk_size -= written;
    if (chunk_size == 0) break;
  }
  if (written == -1) {
    RETURN_ERROR() << "error when writing bin file " << tofino_bin_path_;
  }

  // Context JSON file
  memcpy(&chunk_size, device_data_curr, sizeof(uint32_t));
  device_data_curr += sizeof(uint32_t);
  snprintf(ctx_json_path_, _PI_UPDATE_MAX_TMP_FILENAME_SIZE,
           "%s-ctx.json.XXXXXX", prog_name_);
  int ctx_fd = mkstemp(ctx_json_path_);
  if (ctx_fd == -1) {
    RETURN_ERROR() << "error when trying to create temp context file "
                   << ctx_json_path_;
  }
  while ((written = write(ctx_fd, device_data_curr, chunk_size)) != -1) {
    device_data_curr += written;
    chunk_size -= written;
    if (chunk_size == 0) break;
  }
  if (written == -1) {
    RETURN_ERROR() << "error when writing context file " << ctx_json_path_;
  }

  // BfRt JSON file
  memcpy(&chunk_size, device_data_curr, sizeof(uint32_t));
  device_data_curr += sizeof(uint32_t);
  snprintf(bfrt_file_path_, _PI_UPDATE_MAX_TMP_FILENAME_SIZE,
           "%s-bfrt.json.XXXXXX", prog_name_);
  int bfrt_fd = mkstemp(bfrt_file_path_);
  if (bfrt_fd == -1) {
    RETURN_ERROR() << "error when trying to create temp bfrt file "
                   << bfrt_file_path_;
  }
  while ((written = write(bfrt_fd, device_data_curr, chunk_size)) != -1) {
    device_data_curr += written;
    chunk_size -= written;
    if (chunk_size == 0) break;
  }
  if (written == -1) {
    RETURN_ERROR() << "error when writing bfrt file " << ctx_json_path_;
  }

  char actual_tofino_bin_path_[PATH_MAX + 1];
  char actual_ctx_json_path_[PATH_MAX + 1];
  char actual_bfrt_file_path_[PATH_MAX + 1];
  CHECK_RETURN_IF_FALSE(realpath(tofino_bin_path_, actual_tofino_bin_path_) !=
                        NULL);
  CHECK_RETURN_IF_FALSE(realpath(ctx_json_path_, actual_ctx_json_path_) !=
                        NULL);
  CHECK_RETURN_IF_FALSE(realpath(bfrt_file_path_, actual_bfrt_file_path_) !=
                        NULL);
  ::strncpy(tofino_bin_path_, actual_tofino_bin_path_,
            _PI_UPDATE_MAX_TMP_FILENAME_SIZE);
  ::strncpy(ctx_json_path_, actual_ctx_json_path_,
            _PI_UPDATE_MAX_TMP_FILENAME_SIZE);
  ::strncpy(bfrt_file_path_, actual_bfrt_file_path_,
            _PI_UPDATE_MAX_TMP_FILENAME_SIZE);

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

  // TODO(Yi): Now we only support single P4 program
  device_profile.num_p4_programs = 1;
  bf_p4_program_t* p4_program = &device_profile.p4_programs[0];
  strncpy(p4_program->prog_name, prog_name_, _PI_UPDATE_MAX_NAME_SIZE);
  p4_program->bfrt_json_file = bfrt_file_path_;
  p4_program->num_p4_pipelines = 1;

  // TODO(Yi): Now we applies single pipelines to all HW pipeline
  bf_p4_pipeline_t* pipeline_profile = &p4_program->p4_pipelines[0];
  strcpy(pipeline_profile->p4_pipeline_name, "pipe");
  pipeline_profile->cfg_file = tofino_bin_path_;
  pipeline_profile->runtime_context_file = ctx_json_path_;

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

  // Push pipeline config to the table manager
  BFRT_RETURN_IF_ERROR(
      bfrt_device_manager_->bfRtInfoGet(unit_, prog_name_, &bfrt_info_));
  bfrt_id_mapper_->PushPipelineInfo(p4info_, bfrt_info_);
  bfrt_table_manager_->PushPipelineInfo(p4info_, bfrt_info_);

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
  auto session = bfrt::BfRtSession::sessionCreate();
  session->beginBatch();
  ::util::Status status;
  for (const auto& update : req.updates()) {
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
  }
  session->endBatch(true);
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

// Factory function for creating the instance of the class.
std::unique_ptr<BfRtNode> BfRtNode::CreateInstance(
    BFRuntimeTableManager* bfrt_table_manager,
    ::bfrt::BfRtDevMgr* bfrt_device_manager, int unit) {
  return absl::WrapUnique(
      new BfRtNode(bfrt_table_manager, bfrt_device_manager, unit));
}

BFRuntimeNode::BfRtNode(BFRuntimeTableManager* bfrt_table_manager,
                        ::bfrt::BfRtDevMgr* bfrt_device_manager, int unit)
    : pipeline_initialized_(false),
      initialized_(false),
      bfrt_table_manager_(ABSL_DIE_IF_NULL(bfrt_table_manager)),
      bfrt_device_manager_(ABSL_DIE_IF_NULL(bfrt_device_manager)),
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
