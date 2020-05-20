// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_COMMON_PHAL_INTERFACE_H_
#define STRATUM_HAL_LIB_COMMON_PHAL_INTERFACE_H_

#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>
#include <memory>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/sfp_configurator.h"

namespace stratum {
namespace hal {

// The "PhalInterface" class implements a stratum wrapper around the
// PHAL library. It provides an abstraction layer for accessing all the
// platform peripherals except the switch ASIC. This includes fans, QSFP modules
// power units, etc. An implementation of this class is expected to be self
// contained and hide all the platform-specific details.
class PhalInterface {
 public:
  // TransceiverEvent encapsulates the data to be sent to any transceiver event
  // handlers.
  struct TransceiverEvent {
    int slot;
    int port;
    HwState state;
  };

  // This struct wraps a transceiver module insert/removal event ChannelWriter,
  // a priority, and an id. The priority is used to prioritize ChannelWriter
  // invocation whenever a transceiver module event is received.
  struct TransceiverEventWriter {
    // ChannelWriter for sending messages on transceiver events.
    std::unique_ptr<ChannelWriter<TransceiverEvent>> writer;
    int priority;  // The priority of the Writer.
    int id;        // Unique ID of the Writer.
  };

  // The TransceiverEventWriter comparator used for sorting the container
  // holding the TransceiverEventWriter instances.
  struct TransceiverEventWriterComp {
    bool operator()(const TransceiverEventWriter& a,
                    const TransceiverEventWriter& b) const {
      return a.priority > b.priority;  // high priority first
    }
  };

  // A few predefined priority values that can be used by external functions
  // when calling RegisterTransceiverEventWriter.
  static constexpr int kTransceiverEventWriterPriorityHigh = 100;
  static constexpr int kTransceiverEventWriterPriorityMed = 10;
  static constexpr int kTransceiverEventWriterPriorityLow = 1;

  virtual ~PhalInterface() {}

  // Pushes the chassis config to the class. The ChassisConfig proto includes
  // any generic platform-independent configuration info which PHAL may need.
  // Note that platform-specific configuration is internal to the implementation
  // of this class and is not pushed from outside. This function is expected to
  // perform the coldboot init sequence if PHAL is not yet initialized by the
  // time config is pushed in the coldboot mode.
  virtual ::util::Status PushChassisConfig(const ChassisConfig& config) = 0;

  // Verifies the part of config that this class cares about. This method can
  // be called at any point to verify if the ChassisConfig proto is compatible
  // with PHAL internal info (e.g. makes sure the external SingletonPort
  // messages in ChassisConfig with the same (slot, port) match what PHAL knows
  // about transceiver modules used for that (slot, port)).
  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config) = 0;

  // Fully uninitializes PHAL. Not used for warmboot shutdown. Note that there
  // is no public method to initialize the class. The initialization is done
  // internally after the class instance is created or after
  // PushChassisConfig().
  virtual ::util::Status Shutdown() = 0;

  // TODO(unknown): Add Freeze() and Unfreeze() functions to perform NSF
  // warmboot.

  // Registers a ChannelWriter to send transceiver module (QSFP) insert/removal
  // events. The ChannelWriter sends TransceiverEvent messages which each
  // contain a (slot, port, state) tuple. The priority determines the order of
  // Writes on a transceiver event, in highest-to-lowest priority number order.
  // The returned value is the ID of the Writer, which can be used to unregister
  // it in the future. Note that as soon as a ChannelWriter is registered, we
  // expect a one-time write on all registered Writers for all present
  // transceiver modules.
  virtual ::util::StatusOr<int> RegisterTransceiverEventWriter(
      std::unique_ptr<ChannelWriter<TransceiverEvent>> writer,
      int priority) = 0;

  // Unregisters a transceiver event ChannelWriter given its ID.
  virtual ::util::Status UnregisterTransceiverEventWriter(int id) = 0;

  // Gets the front panel port info by reading the transceiver info EEPROM for
  // the module inserted in the given (slot, port). This method will also
  // return the correct data if the given (slot, port) corresponds to a
  // back plane port where there is no external transceiver module. This method
  // is expected to return error if there is no module is inserted in the
  // given (slot, port) yet.
  virtual ::util::Status GetFrontPanelPortInfo(
      int slot, int port, FrontPanelPortInfo* fp_port_info) = 0;

  // Gets the information about the optical network interface for the given
  // (module, network_interface). This method is expected to return error if
  // there is no related optics module inserted in the
  // given (module, network_interface) yet.
  virtual ::util::Status GetOpticalTransceiverInfo(
      int module, int network_interface,
      OpticalTransceiverInfo* optical_netif_info) = 0;

  // Sets the data from optical_netif_info into the optical transceiver module
  // for the given (module, network_interface). This method is expected to
  // return error if there is no related optics module or network interface
  // inserted yet.
  virtual ::util::Status SetOpticalTransceiverInfo(
      int module, int network_interface,
      const OpticalTransceiverInfo& optical_netif_info) = 0;

  // Set the color/state of a frontpanel port LED, corresponding to the physical
  // port specified by (slot, port, channel). The caller assumes each physical
  // port has one frontpanel port LED, i.e., if a transceiver has 4 channels we
  // assume logically there are 4 LEDs for this transceiver. However, please
  // note the following:
  // 1- Not all platforms support frontpanel port LEDs. If a chassis does
  //    not support port LEDs, a call to this function will be NOOP, with
  //    possibly logging a warning message.
  // 2- Some platforms do not have per-channel LEDs on each transceiver port.
  //    We assume PHAL will aggregate the per-channel LED colors/states into
  //    one LED color/state for that transceiver. The rule for aggregation is
  //    the following:
  //    - If the color and state of all the per channel LEDs are the same, the
  //      aggregate color and state will be the same as all the per channel
  //      color and states.
  //    - If we have a conflict, show "Blinking Amber" if there is at least one
  //      "Blinking Amber" and show "Solid Amber" otherwise.
  // This function shall return an error if and only if there is an internal
  // issue accessing HW.
  virtual ::util::Status SetPortLedState(int slot, int port, int channel,
                                         LedColor color, LedState state) = 0;

 protected:
  // Default constructor. To be called by the Mock class instance or any
  // factory function which uses the default constructor.
  PhalInterface() {}
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_PHAL_INTERFACE_H_
