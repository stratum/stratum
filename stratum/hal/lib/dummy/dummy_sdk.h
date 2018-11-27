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

#ifndef STRATUM_HAL_LIB_DUMMY_DUMMY_SDK_H_
#define STRATUM_HAL_LIB_DUMMY_DUMMY_SDK_H_

#include <grpcpp/grpcpp.h>

#include <functional>
#include <memory>
#include <vector>

#include "absl/time/time.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/integral_types.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/gtl/flat_hash_map.h"
#include "stratum/hal/lib/dummy/dummy_test.pb.h"
#include "stratum/hal/lib/dummy/dummy_test.grpc.pb.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/writer_interface.h"

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

class DummySDK : public Test::Service {
 public:
  ~DummySDK();
  // Override from Test::Service
  // Exposes to external status event generator (e.g. CLI)
  ::grpc::Status
  DeviceStatusUpdate(::grpc::ServerContext* context,
                     const DeviceStatusUpdateRequest* request,
                     DeviceStatusUpdateResponse* response)
  EXCLUSIVE_LOCKS_REQUIRED(device_event_lock_) override;
  ::grpc::Status
  TransceiverEventUpdate(::grpc::ServerContext* context,
                   const TransceiverEventRequest* request,
                   TransceiverEventResponse* response)
  EXCLUSIVE_LOCKS_REQUIRED(xcvr_event_lock_) override;

  // Transceiver event writer.
  ::util::StatusOr<int> RegisterTransceiverEventWriter(
      std::unique_ptr<ChannelWriter<TransceiverEvent>> writer,
      int priority)
  EXCLUSIVE_LOCKS_REQUIRED(sdk_lock_, xcvr_event_lock_);
  ::util::Status UnregisterTransceiverEventWriter(int id)
  EXCLUSIVE_LOCKS_REQUIRED(sdk_lock_, xcvr_event_lock_);

  // Event notify writer for a specific node.
  ::util::Status RegisterNodeEventNotifyWriter(
      uint64 node_id,
      std::shared_ptr<WriterInterface<DummyNodeEventPtr>> writer)
  EXCLUSIVE_LOCKS_REQUIRED(sdk_lock_, device_event_lock_);
  ::util::Status UnregisterNodeEventNotifyWriter(uint64 node_id)
  EXCLUSIVE_LOCKS_REQUIRED(sdk_lock_, device_event_lock_);

  // Event notify writer for chassis
  ::util::Status RegisterChassisEventNotifyWriter(
      std::shared_ptr<WriterInterface<GnmiEventPtr>> writer)
  EXCLUSIVE_LOCKS_REQUIRED(sdk_lock_, device_event_lock_);
  ::util::Status UnregisterChassisEventNotifyWriter()
  EXCLUSIVE_LOCKS_REQUIRED(sdk_lock_, device_event_lock_);

  // Start SDK with test gRPC service
  ::util::Status Start() EXCLUSIVE_LOCKS_REQUIRED(sdk_lock_);

  // Shuts down the SDK, including the gRPC server we use
  ::util::Status Shutdown() EXCLUSIVE_LOCKS_REQUIRED(sdk_lock_);

  static DummySDK* GetSingleton() SHARED_LOCKS_REQUIRED(sdk_lock_);

 private:
  DummySDK();
  ::grpc::Status
  HandlePortStatusUpdate(uint64 node_id,
                         uint64 port_id,
                         ::stratum::hal::DataResponse state_update);
  bool initialized_;
  int xcvr_writer_id_;
  std::vector<PhalInterface::TransceiverEventWriter> xcvr_event_writers_;
  stratum::gtl::flat_hash_map<uint32,
  std::shared_ptr<WriterInterface<DummyNodeEventPtr>>>
    node_event_notify_writers_;
  std::shared_ptr<WriterInterface<GnmiEventPtr>> chassis_event_notify_writer_;
};

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_DUMMY_DUMMY_SDK_H_
