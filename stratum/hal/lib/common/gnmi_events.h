// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_COMMON_GNMI_EVENTS_H_
#define STRATUM_HAL_LIB_COMMON_GNMI_EVENTS_H_

#include <memory>
#include <string>
#include <list>
#include <set>

#include "gnmi/gnmi.grpc.pb.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/timer_daemon.h"
#include "stratum/glue/integral_types.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/gtl/map_util.h"

namespace stratum {
namespace hal {

// Introduction to the contents of this file.
//
// There is a number of gNMI events defined below. The gNMI YANG model tree has
// a lot of leafs and there is no way to guess which type of event should be
// passed to a particular one. A brute-force approach of sending each received
// event to every handler and let it decide if it should do something about it
// works but is very time and CPU resources intensive. The hierarchy of
// EventHandlerList<> templates solves this problem by keeping separate list of
// handlers grouped by the type of event the handler is interested in. This
// way the number of handlers an event is sent to is minimized to those that
// might want to learn about it.
// The idea is simple:
// - an handler knows what events it would like to receive, so, it can register
//   itself with as many per-event lists as there is event types by calling
//   Register() on correct lists.
// - an event knows its type, so, it can call Process() method of correct event
//   handler list, which in turn will call all handlers that are registered.
// C++ template and inheritence magic is used to make the whole process as
// automatic (i.e. without explicit code) as possible.

// A base class for all types of events the gNMI GnmiPublisher handles.
// Allows for using pointer of type GnmiEvent* to reference an event of any
// type.
class GnmiEvent {
 public:
  GnmiEvent() {}
  virtual ~GnmiEvent() {}

  // Triggers processing of this event. The processing is different for each
  // type of an event, so, each type will define its version of this method.
  virtual ::util::Status Process() const = 0;
};
using GnmiEventPtr = std::shared_ptr<GnmiEvent>;

// A helper class that provides implementation of the GnmiEvent::Process()
// method that finds an instance of the EventHandlerList<> class that holds
// references to all handlers interested in this type of events and passes the
// event to this instance for processing.
// This approach shortens the list of handlers that are bothered to check if
// they should do something due to reception of this event.
template <typename E>
class GnmiEventProcess : public GnmiEvent {
 public:
  ::util::Status Process() const override;
};

// A Timer event. Only certain type of subscriptions, like interface statistics,
// handle this type of events.
class TimerEvent : public GnmiEventProcess<TimerEvent> {};

// A Poll event.
class PollEvent : public GnmiEventProcess<PollEvent> {};

// An alarm has been triggered event.
class AlarmEvent : public GnmiEventProcess<AlarmEvent> {
 public:
  AlarmEvent(uint64 time_created, const std::string& info)
      : time_created_(time_created), info_(info) {}

  const std::string& GetInfo() const { return info_; }
  uint64 GetTimeCreated() const { return time_created_; }
  std::string GetSeverity() const { return "CRITICAL"; }
  // In the YANG model of an alarm there is a leaf called `status` that when set
  // to `true` indicates the fact that an alarm has been triggered. The fact
  // that an instance of this class has been created means that an alarm state
  // has been detected so this value will always be 'true'.
  // This method is required by the gNMI framework to support the 'status' leaf.
  bool GetStatus() const { return true; }

 private:
  const uint64 time_created_;
  const std::string info_;
};

// A memory error alarm event.
class MemoryErrorAlarm : public AlarmEvent {
 public:
  MemoryErrorAlarm(uint64 time_created, const std::string& info)
      : AlarmEvent(time_created, info) {}
};

// A flow programming exception alarm event.
class FlowProgrammingExceptionAlarm : public AlarmEvent {
 public:
  FlowProgrammingExceptionAlarm(uint64 time_created, const std::string& info)
      : AlarmEvent(time_created, info) {}
};

// An extended node-specific base class for event of type E the gNMI
// GnmiPublisher handles.
template <typename E>
class PerNodeGnmiEvent : public GnmiEventProcess<E> {
 public:
  explicit PerNodeGnmiEvent(uint64 node_id) : node_id_(node_id) {}
  ~PerNodeGnmiEvent() override {}

  uint64 GetNodeId() const { return node_id_; }

 private:
  uint64 node_id_;
};

// An extended port-specific base class for event of type E the gNMI
// GnmiPublisher handles.
template <typename E>
class PerPortGnmiEvent : public PerNodeGnmiEvent<E> {
 public:
  PerPortGnmiEvent(uint64 node_id, uint32 port_id)
      : PerNodeGnmiEvent<E>(node_id), port_id_(port_id) {}
  ~PerPortGnmiEvent() override {}

  uint64 GetPortId() const { return port_id_; }

 private:
  uint32 port_id_;
};

template <typename E>
class PerOpticalPortGnmiEvent : public GnmiEventProcess<E> {
 public:
  explicit PerOpticalPortGnmiEvent(int32 module, int32 network_interface) :
      module_(module), network_interface_(network_interface) {}
  ~PerOpticalPortGnmiEvent() override {}

  int32 GetModule() const { return module_; }
  int32 GetNetworkInterface() const { return network_interface_; }

 private:
  int32 module_;
  int32 network_interface_;
};

// A Port's Operational State Has Changed event.
class PortOperStateChangedEvent
    : public PerPortGnmiEvent<PortOperStateChangedEvent> {
 public:
  PortOperStateChangedEvent(uint64 node_id, uint32 port_id,
                            const PortState& new_state)
      : PerPortGnmiEvent(node_id, port_id), new_state_(new_state) {}
  ~PortOperStateChangedEvent() override {}

  PortState GetNewState() const { return new_state_; }

 private:
  PortState new_state_;
};

// A Port's Administrative State Has Changed event.
class PortAdminStateChangedEvent
    : public PerPortGnmiEvent<PortAdminStateChangedEvent> {
 public:
  PortAdminStateChangedEvent(uint64 node_id, uint32 port_id,
                             const AdminState& new_state)
      : PerPortGnmiEvent(node_id, port_id), new_state_(new_state) {}
  ~PortAdminStateChangedEvent() override {}

  AdminState GetNewState() const { return new_state_; }

 private:
  AdminState new_state_;
};

// A Port's Loopback State Has Changed event.
class PortLoopbackStateChangedEvent
    : public PerPortGnmiEvent<PortLoopbackStateChangedEvent> {
 public:
  PortLoopbackStateChangedEvent(uint64 node_id, uint32 port_id,
                                const LoopbackState& new_state)
      : PerPortGnmiEvent(node_id, port_id), new_state_(new_state) {}
  ~PortLoopbackStateChangedEvent() override {}

  LoopbackState GetNewState() const { return new_state_; }

 private:
  LoopbackState new_state_;
};

// A Port's Speed expressed in Bits Per Second Has Changed event.
class PortSpeedBpsChangedEvent
    : public PerPortGnmiEvent<PortSpeedBpsChangedEvent> {
 public:
  PortSpeedBpsChangedEvent(uint64 node_id, uint32 port_id, uint64 new_speed_bps)
      : PerPortGnmiEvent(node_id, port_id), new_speed_bps_(new_speed_bps) {}
  ~PortSpeedBpsChangedEvent() override {}

  uint64 GetSpeedBps() const { return new_speed_bps_; }

 private:
  uint64 new_speed_bps_;
};

// A Port's Negotiated Speed expressed in Bits Per Second Has Changed event.
class PortNegotiatedSpeedBpsChangedEvent
    : public PerPortGnmiEvent<PortNegotiatedSpeedBpsChangedEvent> {
 public:
  PortNegotiatedSpeedBpsChangedEvent(uint64 node_id, uint32 port_id,
                                     uint64 new_negotiated_speed_bps)
      : PerPortGnmiEvent(node_id, port_id),
        new_negotiated_speed_bps_(new_negotiated_speed_bps) {}
  ~PortNegotiatedSpeedBpsChangedEvent() override {}

  uint64 GetNegotiatedSpeedBps() const { return new_negotiated_speed_bps_; }

 private:
  uint64 new_negotiated_speed_bps_;
};

// A Port's LACP System Priority Has Changed event.
class PortLacpSystemPriorityChangedEvent
    : public PerPortGnmiEvent<PortLacpSystemPriorityChangedEvent> {
 public:
  PortLacpSystemPriorityChangedEvent(uint64 node_id, uint32 port_id,
                                     uint64 new_system_priority)
      : PerPortGnmiEvent(node_id, port_id),
        new_system_priority_(new_system_priority) {}
  ~PortLacpSystemPriorityChangedEvent() override {}

  uint64 GetSystemPriority() const { return new_system_priority_; }

 private:
  uint64 new_system_priority_;
};

// A Port's MAC Address Has Changed event.
class PortMacAddressChangedEvent
    : public PerPortGnmiEvent<PortMacAddressChangedEvent> {
 public:
  PortMacAddressChangedEvent(uint64 node_id, uint32 port_id,
                             uint64 new_mac_address)
      : PerPortGnmiEvent(node_id, port_id), new_mac_address_(new_mac_address) {}
  ~PortMacAddressChangedEvent() override {}

  uint64 GetMacAddress() const {
    // The MAC address is stores on lower 6 bytes.
    return new_mac_address_ & 0x0000FFFFFFFFFFFFull;
  }

 private:
  uint64 new_mac_address_;
};

// A Port's LACP System ID MAC Address Has Changed event.
class PortLacpRouterMacChangedEvent
    : public PerPortGnmiEvent<PortLacpRouterMacChangedEvent> {
 public:
  PortLacpRouterMacChangedEvent(uint64 node_id, uint32 port_id,
                                  uint64 new_system_id_mac)
      : PerPortGnmiEvent(node_id, port_id),
        new_system_id_mac_(new_system_id_mac) {}
  ~PortLacpRouterMacChangedEvent() override {}

  uint64 GetSystemIdMac() const {
    // The MAC address is stores on lower 6 bytes.
    return new_system_id_mac_ & 0x0000FFFFFFFFFFFFull;
  }

 private:
  uint64 new_system_id_mac_;
};

// A Port's Counters Have Changed event.
class PortCountersChangedEvent
    : public PerPortGnmiEvent<PortCountersChangedEvent> {
 public:
  PortCountersChangedEvent(uint64 node_id, uint32 port_id,
                           const PortCounters& new_counters)
      : PerPortGnmiEvent(node_id, port_id), new_counters_(new_counters) {}
  ~PortCountersChangedEvent() override {}

  uint64 GetInOctets() const { return new_counters_.in_octets(); }
  uint64 GetOutOctets() const { return new_counters_.out_octets(); }
  uint64 GetInUnicastPkts() const { return new_counters_.in_unicast_pkts(); }
  uint64 GetOutUnicastPkts() const { return new_counters_.out_unicast_pkts(); }
  uint64 GetInBroadcastPkts() const {
    return new_counters_.in_broadcast_pkts();
  }
  uint64 GetOutBroadcastPkts() const {
    return new_counters_.out_broadcast_pkts();
  }
  uint64 GetInMulticastPkts() const {
    return new_counters_.in_multicast_pkts();
  }
  uint64 GetOutMulticastPkts() const {
    return new_counters_.out_multicast_pkts();
  }
  uint64 GetInDiscards() const { return new_counters_.in_discards(); }
  uint64 GetOutDiscards() const { return new_counters_.out_discards(); }
  uint64 GetInUnknownProtos() const {
    return new_counters_.in_unknown_protos();
  }
  uint64 GetInErrors() const { return new_counters_.in_errors(); }
  uint64 GetOutErrors() const { return new_counters_.out_errors(); }
  uint64 GetInFcsErrors() const { return new_counters_.in_fcs_errors(); }

 private:
  const PortCounters new_counters_;
};

// A Port's Qos Counters Have Changed event.
class PortQosCountersChangedEvent
    : public PerPortGnmiEvent<PortQosCountersChangedEvent> {
 public:
  PortQosCountersChangedEvent(uint64 node_id, uint32 port_id,
                              const PortQosCounters& new_counters)
      : PerPortGnmiEvent(node_id, port_id), new_counters_(new_counters) {}
  ~PortQosCountersChangedEvent() override {}

  uint64 GetTransmitOctets() const { return new_counters_.out_octets(); }
  uint64 GetTransmitPkts() const { return new_counters_.out_pkts(); }
  uint64 GetDroppedPkts() const { return new_counters_.out_dropped_pkts(); }
  uint32 GetQueueId() const { return new_counters_.queue_id(); }

 private:
  const PortQosCounters new_counters_;
};

// A Port's Forwarding Viable state has changed event.
class PortForwardingViabilityChangedEvent
    : public PerPortGnmiEvent<PortForwardingViabilityChangedEvent> {
 public:
  PortForwardingViabilityChangedEvent(uint64 node_id, uint32 port_id,
                                      const TrunkMemberBlockState& new_state)
      : PerPortGnmiEvent(node_id, port_id), new_state_(new_state) {}
  ~PortForwardingViabilityChangedEvent() override {}

  const TrunkMemberBlockState& GetState() const { return new_state_; }

 private:
  const TrunkMemberBlockState new_state_;
};

// A Port's Health Indicator  state has changed event.
class PortHealthIndicatorChangedEvent
    : public PerPortGnmiEvent<PortHealthIndicatorChangedEvent> {
 public:
  PortHealthIndicatorChangedEvent(uint64 node_id, uint32 port_id,
                                  const HealthState& new_state)
      : PerPortGnmiEvent(node_id, port_id), new_state_(new_state) {}
  ~PortHealthIndicatorChangedEvent() override {}

  const HealthState& GetState() const { return new_state_; }

 private:
  const HealthState new_state_;
};

// A Port's Auto Negotiation status has changed event.
class PortAutonegChangedEvent
    : public PerPortGnmiEvent<PortAutonegChangedEvent> {
 public:
  PortAutonegChangedEvent(uint64 node_id, uint32 port_id,
                                const TriState& new_state)
      : PerPortGnmiEvent(node_id, port_id), new_state_(new_state) {}
  ~PortAutonegChangedEvent() override {}

  const TriState& GetState() const { return new_state_; }

 private:
  const TriState new_state_;
};

// Optical network interface input power changed event.
class OpticalInputPowerChangedEvent
    : public PerOpticalPortGnmiEvent<OpticalInputPowerChangedEvent> {
 public:
  OpticalInputPowerChangedEvent(int32 module, int32 network_interface,
                                const OpticalTransceiverInfo::Power power)
      : PerOpticalPortGnmiEvent(module, network_interface),
        new_input_power_(power) {}
  ~OpticalInputPowerChangedEvent() override {}

  // Actual power values.
  double GetInstant() const { return new_input_power_.instant(); }
  double GetAvg() const { return new_input_power_.avg(); }
  double GetMin() const { return new_input_power_.min(); }
  double GetMax() const { return new_input_power_.max(); }

  // Time values.
  uint64 GetInterval() const { return new_input_power_.interval(); }
  uint64 GetMinTime() const { return new_input_power_.min_time(); }
  uint64 GetMaxTime() const { return new_input_power_.max_time(); }

 private:
  const OpticalTransceiverInfo::Power new_input_power_;
};

// Optical network interface output power changed event.
class OpticalOutputPowerChangedEvent
    : public PerOpticalPortGnmiEvent<OpticalOutputPowerChangedEvent> {
 public:
  OpticalOutputPowerChangedEvent(int32 module, int32 network_interface,
                                 const OpticalTransceiverInfo::Power power)
      : PerOpticalPortGnmiEvent(module, network_interface),
        new_output_power_(power) {}
  ~OpticalOutputPowerChangedEvent() override {}

  // Actual power values.
  double GetInstant() const { return new_output_power_.instant(); }
  double GetAvg() const { return new_output_power_.avg(); }
  double GetMin() const { return new_output_power_.min(); }
  double GetMax() const { return new_output_power_.max(); }

  // Time values.
  uint64 GetInterval() const { return new_output_power_.interval(); }
  uint64 GetMinTime() const { return new_output_power_.min_time(); }
  uint64 GetMaxTime() const { return new_output_power_.max_time(); }

 private:
  const OpticalTransceiverInfo::Power new_output_power_;
};

// Configuration Has Been Pushed event.
class ConfigHasBeenPushedEvent
    : public GnmiEventProcess<ConfigHasBeenPushedEvent> {
 public:
  explicit ConfigHasBeenPushedEvent(const ChassisConfig& new_config)
      : new_config_(new_config) {}
  ~ConfigHasBeenPushedEvent() override {}

  const ChassisConfig& new_config_;
};

using GnmiSubscribeStream =
    ::grpc::ServerReaderWriterInterface<::gnmi::SubscribeResponse,
                                        ::gnmi::SubscribeRequest>;

// A helper class that is used to implement gNMI GET operation using code that
// is designed to handle streaming POLL requests and which expects a
// GnmiSubscribeStream stream as one of its parameters and which is not avaiable
// in the case of a GET request.
class InlineGnmiSubscribeStream : public GnmiSubscribeStream {
 public:
  using WriteFunctor =
      std::function<bool(const ::gnmi::SubscribeResponse& msg)>;
  explicit InlineGnmiSubscribeStream(const WriteFunctor& w) : write_func_(w) {}

  // A method that is called by the OnPoll handers.
  bool Write(const ::gnmi::SubscribeResponse& msg,
             ::grpc::WriteOptions options) override {
    return write_func_(msg);
  }

 private:
  // Required by the interface but not used. Made private to prevent their
  // accidental usage.
  void SendInitialMetadata() override { CHECK(false); }
  bool NextMessageSize(uint32_t* sz) override { CHECK(false); }
  bool Read(::gnmi::SubscribeRequest* msg) override { CHECK(false); }

  // A functor that implements the Write() method.
  WriteFunctor write_func_;
};

using GnmiEventHandler = std::function<::util::Status(
    const GnmiEvent& event, GnmiSubscribeStream* stream)>;

// A class that provides limited (but sufficient) copy-on-write
// functionality - it makes a copy of the original chassis config only if a
// mutable pointer is requested. It is used to avoid unnecessary copies of
// ChassisConfig object when processing gNMI SET requests. Note that the class
// does not take ownership of the pointer passed in the constructor, so, it will
// not be deleted if a copy is made. Note also that it assusmes that the
// ownership of the newly allocated memory will be taken over before the object
// is destroyed.
class CopyOnWriteChassisConfig {
 public:
  explicit CopyOnWriteChassisConfig(ChassisConfig* ptr)
      : copied_(false), delete_active_(false), original_(ptr), active_(ptr) {
    if (ptr == nullptr) {
      // ative_ cannot be nullptr. If it is, allocate a new object.
      active_ = new ChassisConfig();
      delete_active_ = true;
    }
  }

  virtual ~CopyOnWriteChassisConfig() {
    if (delete_active_) {
      // If this is the object that has been allocated by this class and its
      // ownership has not been taken over - delete it to avoid memory leak.
      delete active_;
    }
  }

  bool HasBeenChanged() const { return copied_; }

  // Read operation. Do not make copy.
  const ChassisConfig* operator->() const { return active_; }

  // Read operation. Do not make copy.
  const ChassisConfig& operator*() const { return *active_; }

  // The only way to get mutable/writable access.
  ChassisConfig* writable() {
    // If it has not been copied yet, make a copy.
    if (!copied_) copy();
    return active_;
  }

  // Pass ownership of the allocated buffer and update the state.
  ChassisConfig* PassOwnership() {
    auto result = active_;
    delete_active_ = false;
    active_ = nullptr;
    return result;
  }

 private:
  // Makes a copy of the original chassis config.
  void copy() {
    if (original_ != nullptr) {
      ChassisConfig* new_active = new ChassisConfig();
      *new_active = *original_;
      active_ = new_active;
      delete_active_ = true;
    }
    copied_ = true;
  }

  // Set to true if the ative_ pointer is pointing to a copy.
  bool copied_;
  // Set to true if the buffer pointed by active_ should be deleted by
  // the destructor.
  bool delete_active_;
  ChassisConfig* original_;  // The pointer passed to the constructor.
  ChassisConfig* active_;    // The 'active' pointer. Can be a copy.
};

using GnmiSetHandler = std::function<::util::Status(
    const ::gnmi::Path& path, const ::google::protobuf::Message& val,
    CopyOnWriteChassisConfig* config)>;

using GnmiDeleteHandler = std::function<::util::Status(
    const ::gnmi::Path& path, CopyOnWriteChassisConfig* config)>;

// A class used to keep information about a subscription.
class EventHandlerRecord {
 public:
  // Constructor.
  EventHandlerRecord(const GnmiEventHandler& handler,
                     GnmiSubscribeStream* stream)
      : handler_(handler), stream_(stream) {}
  // Destructor.
  virtual ~EventHandlerRecord() {}

  // Generic processing of an event.
  ::util::Status operator()(const GnmiEvent& event) const {
    auto status = handler_(event, stream_);
    if (status != ::util::OkStatus()) {
      return status;
    }
    return ::util::OkStatus();
  }

  TimerDaemon::DescriptorPtr* mutable_timer() { return &timer_; }

 protected:
  // The handler functor. Is called every time there is an event to handle.
  GnmiEventHandler handler_;
  // A stream to the client (the controller).
  GnmiSubscribeStream* stream_;
  // Not every EventHandler is executed on timer, but some are and this is the
  // handler that is used by the timer sub-system.
  TimerDaemon::DescriptorPtr timer_;
};
using EventHandlerRecordPtr = std::weak_ptr<EventHandlerRecord>;
using SubscriptionHandle = std::shared_ptr<EventHandlerRecord>;

// A base class of the EventHandlerList<event-type> hierarchy. It is needed:
// - to define a virtual method Process() implemented by each specialized event
//   handler list.
// - to allow storing pointers to all specialized instances of event handler
//   list.
// - to implement Register() and UnRegister() methods (to limit code-bloat)
class EventHandlerListBase {
 public:
  // A hierarchy of classes uses this class as base, so, virtual destructor is
  // needed.
  virtual ~EventHandlerListBase() {}

  // A method passing 'event' to all handlers that are registered in the handler
  // list.
  virtual ::util::Status Process(const GnmiEvent& event)
      LOCKS_EXCLUDED(access_lock_) = 0;

  // Adds a event handler to a list of handlers interested in this ('E') type of
  // events.
  ::util::Status Register(const EventHandlerRecordPtr& record)
      LOCKS_EXCLUDED(access_lock_) {
    absl::WriterMutexLock l(&access_lock_);
    handlers_.insert(record);
    return ::util::OkStatus();
  }

  // Removes a event handler from a list of handlers interested in this  ('E')
  // type of events.
  ::util::Status UnRegister(const EventHandlerRecordPtr& record)
      LOCKS_EXCLUDED(access_lock_) {
    absl::WriterMutexLock l(&access_lock_);
    handlers_.erase(record);
    return ::util::OkStatus();
  }

  // Returns the number of handlers that are registered for events of type E.
  size_t GetNumberOfRegisteredHandlers() LOCKS_EXCLUDED(access_lock_) {
    absl::WriterMutexLock l(&access_lock_);
    // To return acurate information remove all expired subscriptions.
    CleanUpInactiveRegistrations();
    // Return the number of still active registrations.
    return handlers_.size();
  }

 protected:
  // Removes pointers that are expired.
  void CleanUpInactiveRegistrations() EXCLUSIVE_LOCKS_REQUIRED(access_lock_) {
    std::list<EventHandlerRecordPtr> entries_to_be_removed;
    for (const auto& entry : handlers_) {
      if (entry.expired()) {
        // The subscription has been silently (without calling UnRegister())
        // canceled by deleting the handle. Make a note of this record and
        // then delete it after all entries in handlers_ are processed.
        entries_to_be_removed.push_back(entry);
      }
    }
    // Remove all subscriptions that have been silently canceled.
    for (const auto& handler : entries_to_be_removed) {
      handlers_.erase(handler);
    }
  }

  // A Mutex used to guard access to the map of pointers to handlers.
  mutable absl::Mutex access_lock_;

  // A set of event handlers that are interested in this ('E') type of events.
  std::set<EventHandlerRecordPtr, std::owner_less<EventHandlerRecordPtr>>
      handlers_ GUARDED_BY(access_lock_);
};

// A class that keeps track of all event handlers that are interested in
// particular type of events: 'E'.
template <typename E>
class EventHandlerList : public EventHandlerListBase {
 public:
  using EventType = E;

  // This is a singleton, so, the only way to create/access its instance is to
  // call this method.
  static EventHandlerList<E>* GetInstance() {
    static EventHandlerList<E>* instance = new EventHandlerList<E>();
    return instance;
  }

  // Processes the event.
  // The dispatcher based on the type of the event to be processed selects one
  // specialized event handler list and calls its Process() method. This method.
  // It goes through the list of registered event handlers and calls each of
  // them with the 'event' to be processed.
  ::util::Status Process(const GnmiEvent& base_event) override {
    absl::WriterMutexLock l(&access_lock_);
    if (const E* event = dynamic_cast<const E*>(&base_event)) {
      VLOG(1) << "Handling " << typeid(E).name();
      CleanUpInactiveRegistrations();
      for (const auto& entry : handlers_) {
        if (auto handler = entry.lock()) {
          (*handler)(*event).IgnoreError();
        }
      }
    } else {
      // This __really__ should never happen!
      LOG(ERROR) << "Incorrectly routed event! " << typeid(base_event).name()
                 << " has been sent to list handling " << typeid(E).name();
    }
    return ::util::OkStatus();
  }

 private:
  // Constructor. Hidden as this class is a singleton.
  EventHandlerList() {}
};

// Implementation of the abstract GnmiEvent::Process() specialized for each type
// of event.
template <typename E>
::util::Status GnmiEventProcess<E>::Process() const {
  // Find the EventHandlerList instance that handles this type of envents and
  // pass this event for processing (calling all registered handlers with this
  // even as an input  parameter)
  return EventHandlerList<E>::GetInstance()->Process(*this);
}

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_GNMI_EVENTS_H_
