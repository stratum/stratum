// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BCM_BCM_SDK_INTERFACE_H_
#define STRATUM_HAL_LIB_BCM_BCM_SDK_INTERFACE_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/channel/channel.h"

namespace stratum {
namespace hal {
namespace barefoot {

// The "BfrtSdeInterface" class in HAL implements a shim layer around the
// Barefoot SDE. it is defined as an abstract class to allow multiple
// implementations: 1- BfrtSdeWrapper: The real implementation which includes
// all the BFRT API
//    calls.
// 2- BfrtSdeMock: Mock class used for unit testing.
class BfrtSdeInterface {
 public:
  // LinkscanEvent encapsulates the information received on a linkscan event.
  struct LinkscanEvent {
    int unit;
    int port;
    PortState state;
  };

  // A few predefined priority values that can be used by external functions
  // when calling RegisterLinkscanEventWriter.
  static constexpr int kLinkscanEventWriterPriorityHigh = 100;
  static constexpr int kLinkscanEventWriterPriorityMed = 10;
  static constexpr int kLinkscanEventWriterPriorityLow = 1;

  virtual ~BfrtSdeInterface() {}

  // Initializes the SDK.
  virtual ::util::Status InitializeSdk(int device_id) = 0;

  // Starts linkscan. If the callback is registered already by calling
  // RegisterLinkscanEventWriter, this will start forwarding the linscan events
  // to the callback.
  virtual ::util::Status StartLinkscan(int device_id) = 0;

  // Stops linkscan.
  virtual ::util::Status StopLinkscan(int device_id) = 0;

  // Create link scan event message
  virtual void OnLinkscanEvent(int device_id, int port,
                               PortState linkstatus) = 0;

  // Registers a Writer through which to send any linkscan events. The message
  // contains a tuple (device_id, port, state), where port refers to the
  // Barefoot SDE logical port. The priority determines the relative priority of
  // the Writer as compared to other registered Writers. When a linkscan event
  // is received, the Writers are invoked in order of highest priority. The
  // returned value is the ID of the Writer. It can be used to unregister the
  // Writer later.
  virtual ::util::StatusOr<int> RegisterLinkscanEventWriter(
      std::unique_ptr<ChannelWriter<LinkscanEvent>> writer, int priority) = 0;

  // Unregisters a linkscan callback given its ID.
  virtual ::util::Status UnregisterLinkscanEventWriter(int id) = 0;

  //
  virtual ::util::Status TxPacket(int device_id, const std::string packet) = 0;
  virtual ::util::Status StartPacketIo(int device_id) = 0;
  virtual ::util::Status StopPacketIo(int device_id) = 0;
  virtual ::util::Status RegisterPacketReceiveWriter(
      int device_id, std::unique_ptr<ChannelWriter<std::string>> writer) = 0;
  virtual ::util::Status UnregisterPacketReceiveWriter(int device_id) = 0;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BfrtSdeInterface() {}
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_SDK_INTERFACE_H_
