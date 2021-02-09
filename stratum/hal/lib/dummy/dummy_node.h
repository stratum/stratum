// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_DUMMY_DUMMY_NODE_H_
#define STRATUM_HAL_LIB_DUMMY_DUMMY_NODE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/dummy/dummy_box.h"
#include "stratum/hal/lib/dummy/dummy_global_vars.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

using Request = stratum::hal::DataRequest::Request;

// Data structure which hold port status.
struct SingletonPortStatus {
  ::stratum::hal::OperStatus oper_status;
  ::stratum::hal::AdminStatus admin_status;
  ::stratum::hal::MacAddress mac_address;
  ::stratum::hal::PortSpeed port_speed;
  ::stratum::hal::PortSpeed negotiated_port_speed;
  ::stratum::hal::MacAddress lacp_router_mac;
  ::stratum::hal::SystemPriority lacp_system_priority;
  ::stratum::hal::PortCounters port_counters;
  ::stratum::hal::ForwardingViability forwarding_viability;
  ::stratum::hal::HealthIndicator health_indicator;
};  // struct SingletonPortStatus

/*
 * Dummy Node implementation.
 * The Node abstraction is representing to actual dataplane (e.g. ASIC, FPGA...)
 * Configured by using P4 Entry and ChassisConfig
 */
class DummyNode {
 public:
  // Update node configuration.
  ::util::Status PushChassisConfig(const ChassisConfig& config)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(node_lock_);

  // Verify node configuration but not update the node.
  ::util::Status VerifyChassisConfig(const ChassisConfig& config)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(node_lock_);

  // Push P4 forwarding pipeline config to the node.
  ::util::Status PushForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(node_lock_);

  // Verify P4 forwarding pipeline config on the node but
  // not push it to the node.
  ::util::Status VerifyForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(node_lock_);

  // Shutdown the node.
  ::util::Status Shutdown() EXCLUSIVE_LOCKS_REQUIRED(chassis_lock)
      LOCKS_EXCLUDED(node_lock_);

  // Freeze and Unfreeze the node.
  // Every public method call to the freezed node should be hanged
  // or returns an error state with proper message.
  ::util::Status Freeze() EXCLUSIVE_LOCKS_REQUIRED(chassis_lock)
      LOCKS_EXCLUDED(node_lock_);
  ::util::Status Unfreeze() EXCLUSIVE_LOCKS_REQUIRED(chassis_lock)
      LOCKS_EXCLUDED(node_lock_);

  // Read and Write forwarding entries to the node.
  // The node should be able to handle tranlation between forwarding entry
  // from P4Runtime and the real binary format of the dataplane.
  ::util::Status WriteForwardingEntries(const ::p4::v1::WriteRequest& req,
                                        std::vector<::util::Status>* results)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(node_lock_);
  ::util::Status ReadForwardingEntries(
      const ::p4::v1::ReadRequest& req,
      WriterInterface<::p4::v1::ReadResponse>* writer,
      std::vector<::util::Status>* details) SHARED_LOCKS_REQUIRED(chassis_lock)
      LOCKS_EXCLUDED(node_lock_);

  // Register/Unregister a packet receive writer.
  // The node should sends P4Runtime PacketIn message to the writer
  // if any message or packet comes from the dataplane.
  // The node may add/remove metadata to/from the packet in message.
  ::util::Status RegisterStreamMessageResponseWriter(
      std::shared_ptr<WriterInterface<::p4::v1::StreamMessageResponse>> writer)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(node_lock_);
  ::util::Status UnregisterStreamMessageResponseWriter()
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(node_lock_);

  // Transmit a packet to the dataplane.
  // The packet out message should contains necessary metadata for the dataplane
  // to handle the packet payload.
  // The node may add/remove metadata to/from the message.
  ::util::Status HandleStreamMessageRequest(
      const ::p4::v1::StreamMessageRequest& request)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(node_lock_);

  // Retrieve port data from this node
  ::util::StatusOr<DataResponse> RetrievePortData(const Request& request)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(node_lock_);

  // Retrieve port qus data from this node
  ::util::StatusOr<DataResponse> RetrievePortQosData(const Request& request)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(node_lock_);

  // Factory function for creating the instance of the class.
  // The DummyNode instance created by ChassisManager when the
  // ChassisConfig pushed.
  static DummyNode* CreateInstance(const uint64 id, const std::string& name,
                                   const int32 slot, const int32 index);

  // Register event notify writer for gNMI events which comes from the node.
  ::util::Status RegisterEventNotifyWriter(
      std::shared_ptr<WriterInterface<GnmiEventPtr>> writer)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(node_lock_);

  // Unregister gNMI event notifu writer from the node.
  ::util::Status UnregisterEventNotifyWriter()
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(node_lock_);

  // Accessors
  uint64 Id() const;
  std::string Name() const;
  int32 Slot() const;
  int32 Index() const;

  // DummyNode is neither copyable nor movable.
  DummyNode(const DummyNode&) = delete;
  DummyNode& operator=(const DummyNode&) = delete;
  DummyNode(DummyNode&&) = delete;
  DummyNode& operator=(DummyNode&&) = delete;

 private:
  // ID for this node.
  uint64 id_;
  std::string name_;
  int32 slot_;
  int32 index_;
  DummyBox* dummy_box_;

  // Should use CreateInstance to create new DummyNode instance
  DummyNode(const uint64 id, const std::string& name, const int32 slot,
            const int32 index);

 protected:
  ::absl::Mutex node_lock_;
  ::absl::flat_hash_map<uint64, SingletonPortStatus> ports_state_;

  // An event writer which updates node status (e.g. port status)
  // And forwards the event.
  class DummyNodeEventWriter : public WriterInterface<DummyNodeEventPtr> {
   public:
    DummyNodeEventWriter(
        DummyNode* dummy_node,
        const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer)
        : dummy_node_(dummy_node), writer_(writer) {}
    bool Write(const DummyNodeEventPtr& msg) override;

   private:
    DummyNode* dummy_node_;  // the dummy_node it belongs to
    std::shared_ptr<WriterInterface<GnmiEventPtr>> writer_;
  };  // class DummyNodeEventWriter
};    // class DummyNode

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_DUMMY_DUMMY_NODE_H_
