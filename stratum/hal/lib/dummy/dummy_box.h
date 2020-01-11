// Copyright 2018-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_DUMMY_DUMMY_BOX_H_
#define STRATUM_HAL_LIB_DUMMY_DUMMY_BOX_H_

#include <functional>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "grpcpp/grpcpp.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/dummy/dummy_test.grpc.pb.h"
#include "stratum/hal/lib/dummy/dummy_test.pb.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

using DeviceStatusUpdateRequest =
::stratum::hal::dummy_switch::DeviceStatusUpdateRequest;
using DeviceStatusUpdateResponse =
::stratum::hal::dummy_switch::DeviceStatusUpdateResponse;
using TransceiverEventRequest =
::stratum::hal::dummy_switch::TransceiverEventRequest;
using TransceiverEventResponse =
::stratum::hal::dummy_switch::TransceiverEventResponse;
using TransceiverEventWriter =
::stratum::hal::PhalInterface::TransceiverEventWriter;
using TransceiverEvent =
::stratum::hal::PhalInterface::TransceiverEvent;
using TransceiverEventWriterComp =
::stratum::hal::PhalInterface::TransceiverEventWriterComp;

struct FindXcvrById:
std::unary_function<TransceiverEventWriter, bool> {
    int id;
    explicit FindXcvrById(int id):id(id) { }
    bool operator()(TransceiverEventWriter const& writer) const {
        return writer.id == id;
    }
};

// Event for passing status update to the node.
// port_id and queue_id are optional
struct DummyNodeEvent {
    uint64 node_id;
    uint64 port_id;
    uint64 queue_id;
    ::stratum::hal::DataResponse state_update;
};

using DummyNodeEventPtr = std::shared_ptr<DummyNodeEvent>;

class DummyBox : public Test::Service {
 public:
  ~DummyBox();
  // Override from Test::Service
  // Exposes to external status event generator (e.g. CLI)
  ::grpc::Status
  DeviceStatusUpdate(::grpc::ServerContext* context,
                     const DeviceStatusUpdateRequest* request,
                     DeviceStatusUpdateResponse* response) override;
  ::grpc::Status
  TransceiverEventUpdate(::grpc::ServerContext* context,
                   const TransceiverEventRequest* request,
                   TransceiverEventResponse* response)
    LOCKS_EXCLUDED(sdk_lock_) override;

  // Transceiver event writer.
  ::util::StatusOr<int> RegisterTransceiverEventWriter(
      std::unique_ptr<ChannelWriter<TransceiverEvent>> writer,
      int priority) LOCKS_EXCLUDED(sdk_lock_);
  ::util::Status UnregisterTransceiverEventWriter(int id)
    LOCKS_EXCLUDED(sdk_lock_);

  // Event notify writer for a specific node.
  ::util::Status RegisterNodeEventNotifyWriter(
      uint64 node_id,
      std::shared_ptr<WriterInterface<DummyNodeEventPtr>> writer)
    LOCKS_EXCLUDED(sdk_lock_);
  ::util::Status UnregisterNodeEventNotifyWriter(uint64 node_id)
    LOCKS_EXCLUDED(sdk_lock_);

  // Event notify writer for chassis
  ::util::Status RegisterChassisEventNotifyWriter(
      std::shared_ptr<WriterInterface<GnmiEventPtr>> writer)
    LOCKS_EXCLUDED(sdk_lock_);
  ::util::Status UnregisterChassisEventNotifyWriter()
    LOCKS_EXCLUDED(sdk_lock_);

  // Start SDK with test gRPC service
  ::util::Status Start() LOCKS_EXCLUDED(sdk_lock_);

  // Shuts down the SDK, including the gRPC server we use
  ::util::Status Shutdown() LOCKS_EXCLUDED(sdk_lock_);

  static DummyBox* GetSingleton();

 private:
  // Hide default constructor
  DummyBox();

  // Method to send port status update to switch interface.
  ::grpc::Status
  HandlePortStatusUpdate(uint64 node_id,
                         uint64 port_id,
                         ::stratum::hal::DataResponse state_update)
    LOCKS_EXCLUDED(sdk_lock_);

  ::absl::Mutex sdk_lock_;  // protects initialized_ xcvr_writer_id_
                            // xcvr_event_writers_ node_event_notify_writers_
                            // chassis_event_notify_writer_
  bool initialized_ GUARDED_BY(sdk_lock_);
  int xcvr_writer_id_ GUARDED_BY(sdk_lock_);
  std::vector<PhalInterface::TransceiverEventWriter> xcvr_event_writers_
  GUARDED_BY(sdk_lock_);
  ::absl::flat_hash_map<uint32,
  std::shared_ptr<WriterInterface<DummyNodeEventPtr>>>
    node_event_notify_writers_
  GUARDED_BY(sdk_lock_);
  std::shared_ptr<WriterInterface<GnmiEventPtr>> chassis_event_notify_writer_
  GUARDED_BY(sdk_lock_);
};

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_DUMMY_DUMMY_BOX_H_
