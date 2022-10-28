// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_TDI_NODE_H_
#define STRATUM_HAL_LIB_TDI_TDI_NODE_H_

#include <memory>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/tdi/tdi.pb.h"
#include "stratum/hal/lib/tdi/tdi_action_profile_manager.h"
#include "stratum/hal/lib/tdi/tdi_counter_manager.h"
#include "stratum/hal/lib/tdi/tdi_packetio_manager.h"
#include "stratum/hal/lib/tdi/tdi_pre_manager.h"
#include "stratum/hal/lib/tdi/tdi_table_manager.h"

namespace stratum {
namespace hal {
namespace tdi {

// The TdiNode class encapsulates all per P4-native node/chip/ASIC
// functionalities, primarily the flow managers. Calls made to this class are
// processed and passed through to the TDI API.
class TdiNode {
 public:
  virtual ~TdiNode();

  virtual ::util::Status PushChassisConfig(const ChassisConfig& config,
                                           uint64 node_id)
      LOCKS_EXCLUDED(lock_);
  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id)
      LOCKS_EXCLUDED(lock_);
  virtual ::util::Status PushForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config);
  virtual ::util::Status SaveForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config) LOCKS_EXCLUDED(lock_);
  virtual ::util::Status CommitForwardingPipelineConfig() LOCKS_EXCLUDED(lock_);
  virtual ::util::Status VerifyForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config) const;
  virtual ::util::Status Shutdown() LOCKS_EXCLUDED(lock_);
  virtual ::util::Status Freeze() LOCKS_EXCLUDED(lock_);
  virtual ::util::Status Unfreeze() LOCKS_EXCLUDED(lock_);
  virtual ::util::Status WriteForwardingEntries(
      const ::p4::v1::WriteRequest& req, std::vector<::util::Status>* results)
      LOCKS_EXCLUDED(lock_);
  virtual ::util::Status ReadForwardingEntries(
      const ::p4::v1::ReadRequest& req,
      WriterInterface<::p4::v1::ReadResponse>* writer,
      std::vector<::util::Status>* details) LOCKS_EXCLUDED(lock_);
  virtual ::util::Status RegisterStreamMessageResponseWriter(
      const std::shared_ptr<WriterInterface<::p4::v1::StreamMessageResponse>>&
          writer) LOCKS_EXCLUDED(lock_);
  virtual ::util::Status UnregisterStreamMessageResponseWriter()
      LOCKS_EXCLUDED(lock_);
  virtual ::util::Status HandleStreamMessageRequest(
      const ::p4::v1::StreamMessageRequest& req) LOCKS_EXCLUDED(lock_);
  // Factory function for creating the instance of the class.
  static std::unique_ptr<TdiNode> CreateInstance(
      TdiTableManager* tdi_table_manager,
      TdiActionProfileManager* tdi_action_profile_manager,
      TdiPacketioManager* tdi_packetio_manager,
      TdiPreManager* tdi_pre_manager,
      TdiCounterManager* tdi_counter_manager,
      TdiSdeInterface* tdi_sde_interface, int device_id,
      // Note: bfrt_node defaults are (true, 1)
      bool initialized = false, uint64 node_id = 0);

  // TdiNode is neither copyable nor movable.
  TdiNode(const TdiNode&) = delete;
  TdiNode& operator=(const TdiNode&) = delete;
  TdiNode(TdiNode&&) = delete;
  TdiNode& operator=(TdiNode&&) = delete;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  TdiNode();

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  TdiNode(TdiTableManager* tdi_table_manager,
          TdiActionProfileManager* tdi_action_profile_manager,
          TdiPacketioManager* tdi_packetio_manager,
          TdiPreManager* tdi_pre_manager,
          TdiCounterManager* tdi_counter_manager,
          TdiSdeInterface* tdi_sde_interface, int device_id,
          bool initialized, uint64 node_id);

  // Write extern entries like ActionProfile, DirectCounter, PortMetadata
  ::util::Status WriteExternEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type type, const ::p4::v1::ExternEntry& entry);

  // Read extern entries like ActionProfile, DirectCounter, PortMetadata
  ::util::Status ReadExternEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::ExternEntry& entry,
      WriterInterface<::p4::v1::ReadResponse>* writer);

  // Callback registered with DeviceMgr to receive stream messages.
  friend void StreamMessageCb(uint64 node_id,
                              p4::v1::StreamMessageResponse* msg, void* cookie);

  // Reader-writer lock used to protect access to node-specific state.
  mutable absl::Mutex lock_;

  // Mutex used for exclusive access to rx_writer_.
  mutable absl::Mutex rx_writer_lock_;

  // Flag indicating whether the pipeline has been pushed.
  bool pipeline_initialized_ GUARDED_BY(lock_);

  // Flag indicating whether the chip is initialized.
  bool initialized_ GUARDED_BY(lock_);

  // Stores pipeline information for this node.
  TdiDeviceConfig tdi_config_ GUARDED_BY(lock_);

  // Pointer to a TdiSdeInterface implementation that wraps all the SDE calls.
  // Not owned by this class.
  TdiSdeInterface* tdi_sde_interface_ = nullptr;

  // Managers. Not owned by this class.
  TdiTableManager* tdi_table_manager_;
  TdiActionProfileManager* tdi_action_profile_manager_;
  TdiPacketioManager* tdi_packetio_manager_;
  TdiPreManager* tdi_pre_manager_;
  TdiCounterManager* tdi_counter_manager_;

  // Logical node ID corresponding to the node/ASIC managed by this class
  // instance. Assigned on PushChassisConfig() and might change during the
  // lifetime of the class.
  uint64 node_id_ GUARDED_BY(lock_);

  // Fixed zero-based BFRT device_id number corresponding to the node/ASIC
  // managed by this class instance. Assigned in the class constructor.
  const int device_id_;
};

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_TDI_NODE_H_
