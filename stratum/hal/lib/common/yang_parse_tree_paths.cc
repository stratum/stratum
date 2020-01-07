// Copyright 2018 Google LLC
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

#include <vector>

#include "stratum/hal/lib/common/yang_parse_tree_paths.h"

#include "gnmi/gnmi.pb.h"
#include "openconfig/openconfig.pb.h"
#include "stratum/hal/lib/common/gnmi_publisher.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/hal/lib/common/openconfig_converter.h"
#include "stratum/lib/constants.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "stratum/glue/gtl/map_util.h"

namespace stratum {
namespace hal {

namespace {

// A helper method that prepares the gNMI message.
::gnmi::SubscribeResponse GetResponse(const ::gnmi::Path& path) {
  ::gnmi::Notification notification;
  uint64 now = absl::GetCurrentTimeNanos();
  notification.set_timestamp(now);
  ::gnmi::Update update;
  *update.mutable_path() = path;
  *notification.add_update() = update;
  ::gnmi::SubscribeResponse resp;
  *resp.mutable_update() = notification;
  return resp;
}

// A helper method that takes 'path' and 'content' and builds a valid message
// of ::gnmi::SubscribeResponse type.
// Multiple data types are sent in this message in the uint_val fields of this
// message therefore this method by default saves the 'content' in this field.
// For types that are saved to other fields a number of specializations of this
// function are provided below.
template <class T>
::gnmi::SubscribeResponse GetResponse(const ::gnmi::Path& path, T contents) {
  ::gnmi::SubscribeResponse resp = GetResponse(path);
  resp.mutable_update()->mutable_update(0)->mutable_val()->set_uint_val(
      contents);
  return resp;
}

// Specialization for 'const char*'.
::gnmi::SubscribeResponse GetResponse(const ::gnmi::Path& path,
                                      const char* contents) {
  ::gnmi::SubscribeResponse resp = GetResponse(path);
  resp.mutable_update()->mutable_update(0)->mutable_val()->set_string_val(
      contents);
  return resp;
}

// Specialization for 'const std::string&'.
::gnmi::SubscribeResponse GetResponse(const ::gnmi::Path& path,
                                      const std::string& contents) {
  ::gnmi::SubscribeResponse resp = GetResponse(path);
  resp.mutable_update()->mutable_update(0)->mutable_val()->set_string_val(
      contents);
  return resp;
}

// Specialization for 'bool'.
::gnmi::SubscribeResponse GetResponse(const ::gnmi::Path& path,
                                      const bool contents) {
  ::gnmi::SubscribeResponse resp = GetResponse(path);
  resp.mutable_update()->mutable_update(0)->mutable_val()->set_bool_val(
      contents);
  return resp;
}

// A helper method that handles writing a response into the output stream.
::util::Status SendResponse(const ::gnmi::SubscribeResponse& resp,
                            GnmiSubscribeStream* stream) {
  if (stream == nullptr) {
    LOG(ERROR) << "Message cannot be sent as the stream pointer is null!";
    return MAKE_ERROR(ERR_INTERNAL) << "stream pointer is null!";
  }
  if (stream->Write(resp, ::grpc::WriteOptions()) == false) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Writing response to stream failed: " << resp.ShortDebugString();
  }
  return ::util::OkStatus();
}

// A helper method that returns a dummy functor that returns 'not supported yet'
// string.
TreeNodeEventHandler UnsupportedFunc() {
  return [](const GnmiEvent& event, const ::gnmi::Path& path,
            GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, "unsupported yet"), stream);
  };
}

// A helper method returning TRUE if the event passed to the handler is a timer.
bool IsTimer(const GnmiEvent& event) {
  const TimerEvent* timer = dynamic_cast<const TimerEvent*>(&event);
  return timer != nullptr;
}

// A helper method returning TRUE if the event passed to the handler is a poll
// request.
bool IsPoll(const GnmiEvent& event) {
  const PollEvent* poll = dynamic_cast<const PollEvent*>(&event);
  return poll != nullptr;
}

// A helper method returning TRUE if the event passed to the handler is a
// notification about a config being pushed..
bool HasConfigBeenPushed(const GnmiEvent& event) {
  const ConfigHasBeenPushedEvent* change =
      dynamic_cast<const ConfigHasBeenPushedEvent*>(&event);
  return change != nullptr;
}

// A family of helper methods that request to change a value of type U on the
// switch using SwitchInterface::RetrieveValue() call. To do its job it
// requires:
// - a pointer to method that gets the message of type T that is part of the
//   DataResponse protobuf and that keeps the value to be returned
//   ('data_response_get_inner_message_func')
// - a pointer to method that checks if the message T is present
//   ('data_response_has_inner_message_func')
// - a pointer to method that returns the value of type U stored in the message
//   ('inner_message_get_field_func')
// - a pointer to method that returns a pointer to mutable DataRequest
//   ('data_request_get_mutable_inner_message_func'); it is needed to build the
//   data retrieval request.

// Port-specific version. Extra parameters needed:
// - node ID ('node_id')
// - port ID ('port_id')
template <typename T, typename U, typename V>
::util::Status SetValue(uint64 node_id, uint64 port_id, YangParseTree* tree,
                        T* (SetRequest::Request::Port::*
                                set_request_get_mutable_inner_message_func)(),
                        void (T::*inner_message_set_field_func)(U),
                        const V& value) {
  // Create a set request.
  SetRequest req;
  auto* request = req.add_requests()->mutable_port();
  request->set_node_id(node_id);
  request->set_port_id(port_id);
  ((request->*set_request_get_mutable_inner_message_func)()
       ->*inner_message_set_field_func)(value);
  // Request the change of the value. The returned status is ignored as there is
  // no way to notify the controller that something went wrong. The error is
  // logged when it is created.
  std::vector<::util::Status> details;
  tree->GetSwitchInterface()->SetValue(node_id, req, &details).IgnoreError();
  // Return status of the operation.
  return (details.size() == 1) ? details.at(0) : ::util::OkStatus();
}

// A family of helper functions that create a functor that reads a value of
// type U from an event of type T. 'get_func' points to the method that reads
// the actual value from the event.

// Port-specific version. Extra parameters needed.
// - node ID ('node_id')
// - port ID ('port_id')
template <typename T, typename U, typename V>
TreeNodeSetHandler GetOnUpdateFunctor(
    uint64 node_id, uint64 port_id, YangParseTree* tree,
    T* (SetRequest::Request::Port::*
            set_request_get_mutable_inner_message_func)(),
    void (T::*inner_message_set_field_func)(U),
    V (::gnmi::TypedValue::*get_value)() const) {
  return [=](const ::gnmi::Path& path, const ::google::protobuf::Message& in,
             CopyOnWriteChassisConfig* config) {
    const ::gnmi::TypedValue* val = static_cast<const ::gnmi::TypedValue*>(&in);
    return SetValue(node_id, port_id, tree,
                    set_request_get_mutable_inner_message_func,
                    inner_message_set_field_func, (val->*get_value)());
  };
}

// A family of helper methods that request a value of type U from the switch
// using SwitchInterface::RetrieveValue() call. To do its job it requires:
// - a pointer to method that gets the message of type T that is part of the
//   DataResponse protobuf and that keeps the value to be returned
//   ('data_response_get_inner_message_func')
// - a pointer to method that checks if the message T is present
//   ('data_response_has_inner_message_func')
// - a pointer to method that returns the value of type U stored in the message
//   ('inner_message_get_field_func')
// - a pointer to method that returns a pointer to mutable DataRequest
//   ('data_request_get_mutable_inner_message_func'); it is needed to build the
//   data retrieval request.

// Port-specific version. Extra parameters needed:
// - node ID ('node_id')
// - port ID ('port_id')
template <typename T, typename U>
U GetValue(
    uint64 node_id, uint32 port_id, YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::Port* (
        DataRequest::Request::*data_request_get_mutable_inner_message_func)(),
    U (T::*inner_message_get_field_func)() const) {
  // Create a data retrieval request.
  DataRequest req;
  auto* request =
      (req.add_requests()->*data_request_get_mutable_inner_message_func)();
  request->set_node_id(node_id);
  request->set_port_id(port_id);
  // In-place definition of method retrieving data from generic response
  // and saving into 'resp' local variable.
  U resp{};
  DataResponseWriter writer(
      [&resp, data_response_get_inner_message_func,
       data_response_has_inner_message_func,
       inner_message_get_field_func](const DataResponse& in) {
        if (!(in.*data_response_has_inner_message_func)()) return false;
        resp = ((in.*data_response_get_inner_message_func)().*
                inner_message_get_field_func)();
        return true;
      });
  // Query the switch. The returned status is ignored as there is no way to
  // notify the controller that something went wrong. The error is logged when
  // it is created.
  tree->GetSwitchInterface()
      ->RetrieveValue(node_id, req, &writer, /* details= */ nullptr)
      .IgnoreError();
  // Return the retrieved value.
  return resp;
}

// Qos-on-a-port-specific version. Extra parameters needed:
// - node ID ('node_id')
// - port ID ('port_id')
// - queue ID ('queue_id')
template <typename T, typename U>
U GetValue(
    uint64 node_id, uint32 port_id, uint32 queue_id, YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::PortQueue* (
        DataRequest::Request::*data_request_get_mutable_inner_message_func)(),
    U (T::*inner_message_get_field_func)() const) {
  // Create a data retrieval request.
  DataRequest req;
  auto* request =
      (req.add_requests()->*data_request_get_mutable_inner_message_func)();
  request->set_node_id(node_id);
  request->set_port_id(port_id);
  request->set_queue_id(queue_id);
  // In-place definition of method retrieving data from generic response
  // and saving into 'resp' local variable.
  U resp{};
  DataResponseWriter writer(
      [&resp, data_response_get_inner_message_func,
       data_response_has_inner_message_func,
       inner_message_get_field_func](const DataResponse& in) {
        if (!(in.*data_response_has_inner_message_func)()) return false;
        resp = ((in.*data_response_get_inner_message_func)().*
                inner_message_get_field_func)();
        return true;
      });
  // Query the switch. The returned status is ignored as there is no way to
  // notify the controller that something went wrong. The error is logged when
  // it is created.
  tree->GetSwitchInterface()
      ->RetrieveValue(node_id, req, &writer, /* details= */ nullptr)
      .IgnoreError();
  // Return the retrieved value.
  return resp;
}

// Chassis-specific version.
template <typename T, typename U>
U GetValue(
    YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::Chassis* (
        DataRequest::Request::*data_request_get_mutable_inner_message_func)(),
    U (T::*inner_message_get_field_func)() const) {
  // Create a data retrieval request.
  DataRequest req;
  *(req.add_requests()->*data_request_get_mutable_inner_message_func)() =
      DataRequest::Request::Chassis();
  // In-place definition of method retrieving data from generic response
  // and saving into 'resp' local variable.
  U resp{};
  DataResponseWriter writer(
      [&resp, data_response_get_inner_message_func,
       data_response_has_inner_message_func,
       inner_message_get_field_func](const DataResponse& in) {
        if (!(in.*data_response_has_inner_message_func)()) return false;
        resp = ((in.*data_response_get_inner_message_func)().*
                inner_message_get_field_func)();
        return true;
      });
  // Query the switch. The returned status is ignored as there is no way to
  // notify the controller that something went wrong. The error is logged when
  // it is created.
  tree->GetSwitchInterface()
      ->RetrieveValue(/* node_id= */ 0, req, &writer, /* details= */ nullptr)
      .IgnoreError();
  // Return the retrieved value.
  return resp;
}

// Node-specific version.
template <typename T, typename U>
U GetValue(
    uint64 node_id, YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::Node* (
        DataRequest::Request::*data_request_get_mutable_inner_message_func)(),
    U (T::*inner_message_get_field_func)() const) {
  // Create a data retrieval request.
  DataRequest req;
  auto* request =
      (req.add_requests()->*data_request_get_mutable_inner_message_func)();
  request->set_node_id(node_id);
  // In-place definition of method retrieving data from generic response
  // and saving into 'resp' local variable.
  U resp{};
  DataResponseWriter writer(
      [&resp, data_response_get_inner_message_func,
       data_response_has_inner_message_func,
       inner_message_get_field_func](const DataResponse& in) {
        if (!(in.*data_response_has_inner_message_func)()) return false;
        resp = ((in.*data_response_get_inner_message_func)().*
                inner_message_get_field_func)();
        return true;
      });
  // Query the switch. The returned status is ignored as there is no way to
  // notify the controller that something went wrong. The error is logged when
  // it is created.
  tree->GetSwitchInterface()
      ->RetrieveValue(node_id, req, &writer, /* details= */ nullptr)
      .IgnoreError();
  // Return the retrieved value.
  return resp;
}

// A helper method that hides the details of registering an event handler into
// per event type handler list.
template <typename E>
TreeNodeEventRegistration RegisterFunc() {
  return [](const EventHandlerRecordPtr& record) {
    return EventHandlerList<E>::GetInstance()->Register(record);
  };
}

// A helper method that hides the details of registering an event handler into
// two per event type handler lists.
template <typename E1, typename E2>
TreeNodeEventRegistration RegisterFunc() {
  return [](const EventHandlerRecordPtr& record) {
    RETURN_IF_ERROR(EventHandlerList<E1>::GetInstance()->Register(record));
    return EventHandlerList<E2>::GetInstance()->Register(record);
  };
}

// A family of helper methods returning a OnPoll functor that reads a value of
// type U from the switch and then sends it to the controller. The value is
// retrieved using GetValue() helper method above which to do its job it
// requires:
// - a pointer to method that gets a protobuf of type T that is part of the
//   DataResponse protobuf and that keeps a field whose value is to be returned
//   ('data_response_get_inner_message_func')
// - a pointer to method that checks if the protobuf of type T is present in the
//   DataResponse protobuf ('data_response_has_inner_message_func')
// - a pointer to method that returns the value of type U stored in the field in
//   the protobuf of type T ('inner_message_get_field_func')
// - a pointer to method that returns a pointer to mutable DataRequest protobuf
//   ('get_mutable_inner_message_func'); it is needed to build the data
//   retrieval request.

// Port-specific version. Extra parameters needed:
// - node ID ('node_id')
// - port ID ('port_id')
template <typename T, typename U>
TreeNodeEventHandler GetOnPollFunctor(
    uint64 node_id, uint32 port_id, YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::Port* (
        DataRequest::Request::*get_mutable_inner_message_func)(),
    U (T::*inner_message_get_field_func)() const) {
  return [=](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    U value =
        GetValue(node_id, port_id, tree, data_response_get_inner_message_func,
                 data_response_has_inner_message_func,
                 get_mutable_inner_message_func, inner_message_get_field_func);
    return SendResponse(GetResponse(path, value), stream);
  };
}

// Qos-queue-on-a-port-specific version. Extra parameters needed:
// - node ID ('node_id')
// - port ID ('port_id')
// - queue ID ('queue_id')
template <typename T, typename U>
TreeNodeEventHandler GetOnPollFunctor(
    uint64 node_id, uint32 port_id, uint32 queue_id, YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::PortQueue* (
        DataRequest::Request::*get_mutable_inner_message_func)(),
    U (T::*inner_message_get_field_func)() const) {
  return [=](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    U value = GetValue(
        node_id, port_id, queue_id, tree, data_response_get_inner_message_func,
        data_response_has_inner_message_func, get_mutable_inner_message_func,
        inner_message_get_field_func);
    return SendResponse(GetResponse(path, value), stream);
  };
}

// Chassis-specific version.
template <typename T, typename U>
TreeNodeEventHandler GetOnPollFunctor(
    YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::Chassis* (
        DataRequest::Request::*get_mutable_inner_message_func)(),
    U (T::*inner_message_get_field_func)() const) {
  return [=](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    U value =
        GetValue(tree, data_response_get_inner_message_func,
                 data_response_has_inner_message_func,
                 get_mutable_inner_message_func, inner_message_get_field_func);
    return SendResponse(GetResponse(path, value), stream);
  };
}

// Node-specific version.
template <typename T, typename U>
TreeNodeEventHandler GetOnPollFunctor(
    uint64 node_id, YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::Node* (
        DataRequest::Request::*get_mutable_inner_message_func)(),
    U (T::*inner_message_get_field_func)() const) {
  return [=](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    U value =
        GetValue(node_id, tree, data_response_get_inner_message_func,
                 data_response_has_inner_message_func,
                 get_mutable_inner_message_func, inner_message_get_field_func);
    return SendResponse(GetResponse(path, value), stream);
  };
}

// A family of helper methods returning a OnPoll functor that reads a value of
// type U from the switch and then post-processes it before sending it to the
// controller. The value is retrieved using GetValue() helper method above which
// to do its job it requires:
// - a pointer to method that gets a protobuf of type T that is part of the
//   DataResponse protobuf and that keeps a field whose value is to be returned
//   ('data_response_get_inner_message_func')
// - a pointer to method that checks if the protobuf of type T is present in the
//   DataResponse protobuf ('data_response_has_inner_message_func')
// - a pointer to method that returns the value of type U stored in the field in
//   the protobuf of type T ('inner_message_get_field_func')
// - a pointer to method that returns a pointer to mutable DataRequest protobuf
//   ('get_mutable_inner_message_func'); it is needed to build the data
//   retrieval request.
// The retrieved value before being sent to the controller is processed by
// a method pointed by 'process_func' that gets the retrieved value of type U,
// casts it to type W and converts it into another gNMI-compliant value of type
// V.

// Port-specific version. Extra parameters needed.
// - node ID ('node_id')
// - port ID ('port_id')
template <typename T, typename U, typename V, typename W>
TreeNodeEventHandler GetOnPollFunctor(
    uint64 node_id, uint32 port_id, YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::Port* (
        DataRequest::Request::*get_mutable_inner_message_func)(),
    U (T::*inner_message_get_field_func)() const, V (*process_func)(const W&)) {
  return [=](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    U value =
        GetValue(node_id, port_id, tree, data_response_get_inner_message_func,
                 data_response_has_inner_message_func,
                 get_mutable_inner_message_func, inner_message_get_field_func);
    return SendResponse(GetResponse(path, (*process_func)(value)), stream);
  };
}

// Qos-queue-on-a-port-specific version. Extra parameters needed:
// - node ID ('node_id')
// - port ID ('port_id')
// - queue ID ('queue_id')
template <typename T, typename U, typename V, typename W>
TreeNodeEventHandler GetOnPollFunctor(
    uint64 node_id, uint32 port_id, uint32 queue_id, YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::PortQueue* (
        DataRequest::Request::*get_mutable_inner_message_func)(),
    U (T::*inner_message_get_field_func)() const, V (*process_func)(const W&)) {
  return [=](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    U value = GetValue(
        node_id, port_id, queue_id, tree, data_response_get_inner_message_func,
        data_response_has_inner_message_func, get_mutable_inner_message_func,
        inner_message_get_field_func);
    return SendResponse(GetResponse(path, (*process_func)(value)), stream);
  };
}

// Chassis-specific version.
template <typename T, typename U, typename V, typename W>
TreeNodeEventHandler GetOnPollFunctor(
    YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::Chassis* (
        DataRequest::Request::*get_mutable_inner_message_func)(),
    U (T::*inner_message_get_field_func)() const, V (*process_func)(const W&)) {
  return [=](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    U value =
        GetValue(tree, data_response_get_inner_message_func,
                 data_response_has_inner_message_func,
                 get_mutable_inner_message_func, inner_message_get_field_func);
    return SendResponse(GetResponse(path, (*process_func)(value)), stream);
  };
}

// A family of helper functions that create a functor that reads a value of
// type U from an event of type T. 'get_func' points to the method that reads
// the actual value from the event.

// Port-specific version. Extra parameters needed.
// - node ID ('node_id')
// - port ID ('port_id')
template <typename T, typename U>
TreeNodeEventHandler GetOnChangeFunctor(uint64 node_id, uint32 port_id,
                                        U (T::*get_func_ptr)() const) {
  return [node_id, port_id, get_func_ptr](const GnmiEvent& event,
                                          const ::gnmi::Path& path,
                                          GnmiSubscribeStream* stream) {
    // For now, we are interested in events of type T only!
    const T* change = dynamic_cast<const T*>(&event);
    if (change == nullptr || change->GetPortId() != port_id) {
      // This is not the event you are looking for...
      return ::util::OkStatus();
    }
    return SendResponse(GetResponse(path, (change->*get_func_ptr)()), stream);
  };
}

// Qos-queue-on-a-port-specific version. Extra parameters needed:
// - node ID ('node_id')
// - port ID ('port_id')
// - queue ID ('queue_id')
template <typename T, typename U>
TreeNodeEventHandler GetOnChangeFunctor(uint64 node_id, uint32 port_id,
                                        uint32 queue_id,
                                        U (T::*get_func_ptr)() const) {
  return [=](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    // For now, we are interested in events of type T only!
    const T* change = dynamic_cast<const T*>(&event);
    if (change == nullptr || change->GetPortId() != port_id ||
        change->GetQueueId() != queue_id) {
      // This is not the event you are looking for...
      return ::util::OkStatus();
    }
    return SendResponse(GetResponse(path, (change->*get_func_ptr)()), stream);
  };
}

// Chassis-specific version.
template <typename T, typename U>
TreeNodeEventHandler GetOnChangeFunctor(U (T::*get_func_ptr)() const) {
  return [get_func_ptr](const GnmiEvent& event, const ::gnmi::Path& path,
                        GnmiSubscribeStream* stream) {
    // For now, we are interested in events of type T only!
    const T* change = dynamic_cast<const T*>(&event);
    if (change == nullptr) {
      // This is not the event you are looking for...
      return ::util::OkStatus();
    }
    return SendResponse(GetResponse(path, (change->*get_func_ptr)()), stream);
  };
}

// A family of helper functions that create a functor that reads a value of type
// U from an event of type T. 'get_func_ptr' points to the method that reads the
// actual value from the event. 'process_func_ptr' points to a function that
// post-processes the value read by the 'get_func_ptr' method before passing it
// to the function that builds the gNMI response message.

// Port-specific version Extra parameters needed.
// - node ID ('node_id')
// - port ID ('port_id').
template <typename T, typename U, typename V, typename W>
TreeNodeEventHandler GetOnChangeFunctor(uint64 node_id, uint32 port_id,
                                        U (T::*get_func_ptr)() const,
                                        V (*process_func_ptr)(const W&)) {
  return [node_id, port_id, get_func_ptr, process_func_ptr](
             const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    // For now, we are interested in events of type T only!
    const T* change = dynamic_cast<const T*>(&event);
    if (change == nullptr || change->GetPortId() != port_id) {
      // This is not the event you are looking for...
      return ::util::OkStatus();
    }
    return SendResponse(
        GetResponse(path, (*process_func_ptr)((change->*get_func_ptr)())),
        stream);
  };
}

// Chassis-specific version.
template <typename T, typename U, typename V, typename W>
TreeNodeEventHandler GetOnChangeFunctor(U (T::*get_func_ptr)() const,
                                        V (*process_func_ptr)(const W&)) {
  return [get_func_ptr, process_func_ptr](const GnmiEvent& event,
                                          const ::gnmi::Path& path,
                                          GnmiSubscribeStream* stream) {
    // For now, we are interested in events of type T only!
    const T* change = dynamic_cast<const T*>(&event);
    if (change == nullptr) {
      // This is not the event you are looking for...
      return ::util::OkStatus();
    }
    return SendResponse(
        GetResponse(path, (*process_func_ptr)((change->*get_func_ptr)())),
        stream);
  };
}

////////////////////////////////////////////////////////////////////////////////
// /
void SetUpRoot(TreeNode* node, YangParseTree* tree) {
  auto poll_functor = UnsupportedFunc();
  auto on_change_functor = UnsupportedFunc();
  auto on_replace_functor =
      [](const ::gnmi::Path& path, const ::google::protobuf::Message& val,
         CopyOnWriteChassisConfig* config) -> ::util::Status {
    auto* typed_value = static_cast<const gnmi::TypedValue*>(&val);
    if (!typed_value) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Not a TypedValue!";
    }
    if (typed_value->value_case() != gnmi::TypedValue::kBytesVal) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Expects a bytes stream!";
    }
    openconfig::Device in;
    // Deserialize the input proto.
    CHECK_RETURN_IF_FALSE(in.ParseFromString(typed_value->bytes_val()));
    // Convert the input proto into the internal format.
    ASSIGN_OR_RETURN(*config->writable(),
                     OpenconfigConverter::OcDeviceToChassisConfig(in));

    return ::util::OkStatus();
  };
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnReplaceHandler(on_replace_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/last-change
void SetUpInterfacesInterfaceStateLastChange(TreeNode* node) {
  auto poll_functor = UnsupportedFunc();
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/ifindex
void SetUpInterfacesInterfaceStateIfindex(uint32 port_id, TreeNode* node) {
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler([port_id](const GnmiEvent& event,
                                    const ::gnmi::Path& path,
                                    GnmiSubscribeStream* stream) {
        return SendResponse(GetResponse(path, port_id), stream);
      })
      ->SetOnPollHandler([port_id](const GnmiEvent& event,
                                   const ::gnmi::Path& path,
                                   GnmiSubscribeStream* stream) {
        return SendResponse(GetResponse(path, port_id), stream);
      })
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/name
void SetUpInterfacesInterfaceStateName(const std::string& name,
                                       TreeNode* node) {
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler([name](const GnmiEvent& event,
                                 const ::gnmi::Path& path,
                                 GnmiSubscribeStream* stream) {
        return SendResponse(GetResponse(path, name), stream);
      })
      ->SetOnPollHandler([name](const GnmiEvent& event,
                                const ::gnmi::Path& path,
                                GnmiSubscribeStream* stream) {
        return SendResponse(GetResponse(path, name), stream);
      })
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/oper-status
void SetUpInterfacesInterfaceStateOperStatus(uint64 node_id, uint32 port_id,
                                             TreeNode* node,
                                             YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(node_id, port_id, tree, &DataResponse::oper_status,
                       &DataResponse::has_oper_status,
                       &DataRequest::Request::mutable_oper_status,
                       &OperStatus::state, ConvertPortStateToString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortOperStateChangedEvent::GetNewState,
      ConvertPortStateToString);
  auto register_functor = RegisterFunc<PortOperStateChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/admin-status
void SetUpInterfacesInterfaceStateAdminStatus(uint64 node_id, uint32 port_id,
                                              TreeNode* node,
                                              YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(node_id, port_id, tree, &DataResponse::admin_status,
                       &DataResponse::has_admin_status,
                       &DataRequest::Request::mutable_admin_status,
                       &AdminStatus::state, ConvertAdminStateToString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortAdminStateChangedEvent::GetNewState,
      ConvertAdminStateToString);
  auto register_functor = RegisterFunc<PortAdminStateChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/hardware-port
void SetUpInterfacesInterfaceStateHardwarePort(uint64 node_id, uint32 port_id,
                                               TreeNode* node,
                                               YangParseTree* tree) {
  // Regular method using a template cannot be used to get the OnPoll functor as
  // std::string fields are treated differently by the PROTO-to-C++ generator:
  // the getter returns "const std::string&" instead of "string" which leads to
  // the template compilation error.
  auto poll_functor = [node_id, port_id, tree](const GnmiEvent& event,
                                      const ::gnmi::Path& path,
                                      GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    auto* request = req.add_requests()->mutable_hardware_port();
    request->set_node_id(node_id);
    request->set_port_id(port_id);
    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    std::string resp{};
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_hardware_port()) return false;
      resp = in.hardware_port().name();
      return true;
    });
    // Query the switch. The returned status is ignored as there is no way to
    // notify the controller that something went wrong. The error is logged when
    // it is created.
    tree->GetSwitchInterface()
        ->RetrieveValue(node_id, req, &writer, /* details= */ nullptr)
        .IgnoreError();
    return SendResponse(GetResponse(path, resp), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/health-indicator
//
void SetUpInterfacesInterfaceStateHealthIndicator(uint64 node_id,
                                                  uint32 port_id,
                                                  TreeNode* node,
                                                  YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(node_id, port_id, tree, &DataResponse::health_indicator,
                       &DataResponse::has_health_indicator,
                       &DataRequest::Request::mutable_health_indicator,
                       &HealthIndicator::state, ConvertHealthStateToString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortHealthIndicatorChangedEvent::GetState,
      ConvertHealthStateToString);
  auto register_functor = RegisterFunc<PortHealthIndicatorChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/config/health-indicator
//
void SetUpInterfacesInterfaceConfigHealthIndicator(const std::string& state,
                                                   uint64 node_id,
                                                   uint64 port_id,
                                                   TreeNode* node,
                                                   YangParseTree* tree) {
  auto poll_functor = [state](const GnmiEvent& event, const ::gnmi::Path& path,
                              GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when it
    // was configured!
    return SendResponse(GetResponse(path, state), stream);
  };
  auto on_set_functor =
      [node_id, port_id, node, tree](
          const ::gnmi::Path& path, const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
    const gnmi::TypedValue* typed_val =
        dynamic_cast<const gnmi::TypedValue*>(&val);
    if (typed_val == nullptr) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "not a TypedValue message!";
    }
    std::string state_string = typed_val->string_val();
    HealthState typed_state;
    if (state_string.compare("BAD") == 0) {
      typed_state = HealthState::HEALTH_STATE_BAD;
    } else if (state_string.compare("GOOD") == 0) {
      typed_state = HealthState::HEALTH_STATE_GOOD;
    } else if (state_string.compare("UNKNOWN") == 0) {
      typed_state = HealthState::HEALTH_STATE_UNKNOWN;
    } else {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "wrong value!";
    }

    // Set the value.
    auto status = SetValue(node_id, port_id, tree,
                           &SetRequest::Request::Port::mutable_health_indicator,
                           &HealthIndicator::set_state, typed_state);
    if (status != ::util::OkStatus()) {
      return status;
    }

    // Update the YANG parse tree.
    auto poll_functor = [state_string](const GnmiEvent& event,
                                       const ::gnmi::Path& path,
                                       GnmiSubscribeStream* stream) {
      // This leaf represents configuration data. Return what was known when
      // it was configured!
      return SendResponse(GetResponse(path, state_string), stream);
    };
    node->SetOnTimerHandler(poll_functor)->SetOnPollHandler(poll_functor);

    // Trigger change notification.
    tree->SendNotification(GnmiEventPtr(
        new PortHealthIndicatorChangedEvent(node_id, port_id, typed_state)));

    return ::util::OkStatus();
  };
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortHealthIndicatorChangedEvent::GetState,
      ConvertHealthStateToString);
  auto register_functor = RegisterFunc<PortHealthIndicatorChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/config/enabled
//
void SetUpInterfacesInterfaceConfigEnabled(const bool state,
                                           uint64 node_id, uint32 port_id,
                                           TreeNode* node,
                                           YangParseTree* tree) {
  auto poll_functor = [state](const GnmiEvent& event, const ::gnmi::Path& path,
                              GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when it
    // was configured!
    return SendResponse(GetResponse(path, state), stream);
  };
  auto on_set_functor =
      [node_id, port_id, node, tree](
          const ::gnmi::Path& path, const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
    const gnmi::TypedValue* typed_val =
        dynamic_cast<const gnmi::TypedValue*>(&val);
    if (typed_val == nullptr) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "not a TypedValue message!";
    }
    bool state_bool = typed_val->bool_val();
    AdminState typed_state = state_bool ? AdminState::ADMIN_STATE_ENABLED:
                                          AdminState::ADMIN_STATE_DISABLED;

    // Set the value.
    auto status = SetValue(node_id, port_id, tree,
                           &SetRequest::Request::Port::mutable_admin_status,
                           &AdminStatus::set_state, typed_state);
    if (status != ::util::OkStatus()) {
      return status;
    }

    // Update the chassis config
    ChassisConfig* new_config = config->writable();
    for (int i = 0; i < new_config->singleton_ports_size(); i++) {
        auto* singleton_port = new_config->mutable_singleton_ports(i);
        if (singleton_port->node() == node_id &&
            singleton_port->id() == port_id) {  // NOLINTNEXTLINE
            singleton_port->mutable_config_params()->set_admin_state(typed_state);
            break;
        }
    }

    // Update the YANG parse tree.
    auto poll_functor = [state_bool](const GnmiEvent& event,
                                       const ::gnmi::Path& path,
                                       GnmiSubscribeStream* stream) {
      // This leaf represents configuration data. Return what was known when
      // it was configured!
      return SendResponse(GetResponse(path, state_bool), stream);
    };
    node->SetOnTimerHandler(poll_functor)
        ->SetOnPollHandler(poll_functor);

    return ::util::OkStatus();
  };
  auto register_functor = RegisterFunc<PortAdminStateChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortAdminStateChangedEvent::GetNewState,
      IsAdminStateEnabled);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /lacp/interfaces/interface[name=<name>]/state/system-id-mac
void SetUpLacpInterfacesInterfaceStateSystemIdMac(uint64 node_id,
                                                  uint32 port_id,
                                                  TreeNode* node,
                                                  YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(node_id, port_id, tree, &DataResponse::lacp_router_mac,
                       &DataResponse::has_lacp_router_mac,
                       &DataRequest::Request::mutable_lacp_router_mac,
                       &MacAddress::mac_address, MacAddressToYangString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortLacpRouterMacChangedEvent::GetSystemIdMac,
      MacAddressToYangString);
  auto register_functor = RegisterFunc<PortLacpRouterMacChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /lacp/interfaces/interface[name=<name>]/state/system-priority
void SetUpLacpInterfacesInterfaceStateSystemPriority(uint64 node_id,
                                                     uint32 port_id,
                                                     TreeNode* node,
                                                     YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, tree, &DataResponse::lacp_system_priority,
      &DataResponse::has_lacp_system_priority,
      &DataRequest::Request::mutable_lacp_system_priority,
      &SystemPriority::priority);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortLacpSystemPriorityChangedEvent::GetSystemPriority);
  auto register_functor = RegisterFunc<PortLacpSystemPriorityChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/config/mac-address
void SetUpInterfacesInterfaceEthernetConfigMacAddress(uint64 node_id,
                                                      uint32 port_id,
                                                      uint64 mac_address,
                                                      TreeNode* node,
                                                      YangParseTree* tree) {
  auto poll_functor = [mac_address](const GnmiEvent& event,
                                    const ::gnmi::Path& path,
                                    GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when it
    // was configured!
    return SendResponse(GetResponse(path, MacAddressToYangString(mac_address)),
                        stream);
  };
  auto on_change_functor = UnsupportedFunc();

  auto on_set_functor =
      [node_id, port_id, node, tree](
          const ::gnmi::Path& path,
          const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
    const gnmi::TypedValue* typed_val =
        dynamic_cast<const gnmi::TypedValue*>(&val);
    if (typed_val == nullptr) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "not a TypedValue message!";
    }

    std::string mac_address_string = typed_val->string_val();
    if (!IsMacAddressValid(mac_address_string)) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "wrong value!";
    }

    ::google::protobuf::uint64 mac_address =
        YangStringToMacAddress(mac_address_string);
    // Set the value.
    auto status = SetValue(node_id, port_id, tree,
                           &SetRequest::Request::Port::mutable_mac_address,
                           &MacAddress::set_mac_address, mac_address);
    if (status != ::util::OkStatus()) {
      return status;
    }

    // Update the chassis config
    ChassisConfig* new_config = config->writable();
    for (int i = 0; i < new_config->singleton_ports_size(); i++) {
      auto* singleton_port = new_config->mutable_singleton_ports(i);
      if (singleton_port->node() == node_id &&
          singleton_port->id() == port_id) {
        singleton_port->mutable_config_params()->set_mac_address(mac_address);
        break;
      }
    }

    // Update the YANG parse tree.
    auto poll_functor = [mac_address](const GnmiEvent& event,
                                      const ::gnmi::Path& path,
                                      GnmiSubscribeStream* stream) {
      // This leaf represents configuration data. Return what was known when it
      // was configured!
      return SendResponse(
          GetResponse(path, MacAddressToYangString(mac_address)), stream);
    };
    node->SetOnTimerHandler(poll_functor)
        ->SetOnPollHandler(poll_functor);

    // Trigger change notification.
    tree->SendNotification(GnmiEventPtr(
        new PortMacAddressChangedEvent(node_id, port_id, mac_address)));

    return ::util::OkStatus();
  };

  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/config/port-speed
void SetUpInterfacesInterfaceEthernetConfigPortSpeed(uint64 node_id,
                                                     uint32 port_id,
                                                     uint64 speed_bps,
                                                     TreeNode* node,
                                                     YangParseTree* tree) {
  auto poll_functor = [speed_bps](const GnmiEvent& event,
                                  const ::gnmi::Path& path,
                                  GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when it
    // was configured!
    return SendResponse(GetResponse(path, ConvertSpeedBpsToString(speed_bps)),
                        stream);
  };
  auto on_set_functor =
      [node_id, port_id, node, tree](
          const ::gnmi::Path& path, const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
    const gnmi::TypedValue* typed_val =
        dynamic_cast<const gnmi::TypedValue*>(&val);
    if (typed_val == nullptr) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "not a TypedValue message!";
    }
    std::string speed_string = typed_val->string_val();
    ::google::protobuf::uint64 speed_bps =
        ConvertStringToSpeedBps(speed_string);
    if (speed_bps == 0) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "wrong value!";
    }

    // Set the value.
    auto status = SetValue(node_id, port_id, tree,
                           &SetRequest::Request::Port::mutable_port_speed,
                           &PortSpeed::set_speed_bps, speed_bps);
    if (status != ::util::OkStatus()) {
      return status;
    }

    // Update the chassis config
    ChassisConfig* new_config = config->writable();
    for (int i = 0; i < new_config->singleton_ports_size(); i++) {
        auto* singleton_port = new_config->mutable_singleton_ports(i);
        if (singleton_port->node() == node_id &&
            singleton_port->id() == port_id) {
            singleton_port->set_speed_bps(speed_bps);
            break;
        }
    }

    // Update the YANG parse tree.
    auto poll_functor = [speed_string](const GnmiEvent& event,
                                       const ::gnmi::Path& path,
                                       GnmiSubscribeStream* stream) {
      // This leaf represents configuration data. Return what was known when
      // it was configured!
      return SendResponse(GetResponse(path, speed_string), stream);
    };
    node->SetOnTimerHandler(poll_functor)
        ->SetOnPollHandler(poll_functor);

    return ::util::OkStatus();
  };
  auto register_functor = RegisterFunc<PortSpeedBpsChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortSpeedBpsChangedEvent::GetSpeedBps,
      ConvertSpeedBpsToString);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/config/auto-negotiate
void SetUpInterfacesInterfaceEthernetConfigAutoNegotiate(uint64 node_id,
                                                         uint32 port_id,
                                                         bool autoneg_status,
                                                         TreeNode* node,
                                                         YangParseTree* tree) {
  auto poll_functor = [autoneg_status](const GnmiEvent& event,
                                       const ::gnmi::Path& path,
                                       GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when it
    // was configured!
    return SendResponse(GetResponse(path, autoneg_status), stream);
  };
  auto on_set_functor =
      [node_id, port_id, node, tree](
          const ::gnmi::Path& path, const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
    const gnmi::TypedValue* typed_val =
        dynamic_cast<const gnmi::TypedValue*>(&val);
    if (typed_val == nullptr) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "not a TypedValue message!";
    }
    bool autoneg_bool = typed_val->bool_val();
    TriState autoneg_status = autoneg_bool ?
                                TriState::TRI_STATE_TRUE :
                                TriState::TRI_STATE_FALSE;

    // Set the value.
    auto status = SetValue(node_id, port_id, tree,
                           &SetRequest::Request::Port::mutable_autoneg_status,
                           &AutonegotiationStatus::set_state, autoneg_status);
    if (status != ::util::OkStatus()) {
      return status;
    }

    // Update the chassis config
    ChassisConfig* new_config = config->writable();
    for (int i = 0; i < new_config->singleton_ports_size(); i++) {
        auto* singleton_port = new_config->mutable_singleton_ports(i);
        if (singleton_port->node() == node_id &&
            singleton_port->id() == port_id) {  // NOLINTNEXTLINE
            singleton_port->mutable_config_params()->set_autoneg(autoneg_status);
            break;
        }
    }

    // Update the YANG parse tree.
    auto poll_functor = [autoneg_bool](const GnmiEvent& event,
                                       const ::gnmi::Path& path,
                                       GnmiSubscribeStream* stream) {
      // This leaf represents configuration data. Return what was known when
      // it was configured!
      return SendResponse(GetResponse(path, autoneg_bool), stream);
    };
    node->SetOnTimerHandler(poll_functor)
        ->SetOnPollHandler(poll_functor);

    return ::util::OkStatus();
  };
  auto register_functor = RegisterFunc<PortAutonegChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortAutonegChangedEvent::GetState,
      IsPortAutonegEnabled);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/config/forwarding-viable
void SetUpInterfacesInterfaceEthernetConfigForwardingViability(uint64 node_id,
        uint32 port_id, bool forwarding_viability, TreeNode* node,
        YangParseTree* tree) {
  auto poll_functor = [forwarding_viability](const GnmiEvent& event,
                                             const ::gnmi::Path& path,
                                             GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when it
    // was configured!
    return SendResponse(GetResponse(path, forwarding_viability), stream);
  };
  auto on_set_functor =
      [node_id, port_id, node, tree](
          const ::gnmi::Path& path, const ::google::protobuf::Message& val,
          CopyOnWriteChassisConfig* config) -> ::util::Status {
        const gnmi::TypedValue* typed_val =
            dynamic_cast<const gnmi::TypedValue*>(&val);
        if (typed_val == nullptr) {
          return MAKE_ERROR(ERR_INVALID_PARAM) << "not a TypedValue message!";
        }
        TrunkMemberBlockState new_forwarding_viability = typed_val->bool_val() ?
            TRUNK_MEMBER_BLOCK_STATE_FORWARDING :
                TRUNK_MEMBER_BLOCK_STATE_BLOCKED;
        auto status = SetValue(node_id, port_id, tree,
            &SetRequest::Request::Port::mutable_forwarding_viability,
            &ForwardingViability::set_state, new_forwarding_viability);

        if (status != ::util::OkStatus()) {
          return status;
        }

        // Update the YANG parse tree.
        auto poll_functor = [new_forwarding_viability](const GnmiEvent& event,
                                           const ::gnmi::Path& path,
                                           GnmiSubscribeStream* stream) {
          return SendResponse(GetResponse(path,
                  ConvertTrunkMemberBlockStateToBool(new_forwarding_viability)),
                  stream);
        };
        node->SetOnTimerHandler(poll_functor)->SetOnPollHandler(poll_functor);

        return ::util::OkStatus();
  };

  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor)
      ->SetOnUpdateHandler(on_set_functor)
      ->SetOnReplaceHandler(on_set_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/state/mac-address
void SetUpInterfacesInterfaceEthernetStateMacAddress(uint64 node_id,
                                                     uint32 port_id,
                                                     TreeNode* node,
                                                     YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(node_id, port_id, tree, &DataResponse::mac_address,
                       &DataResponse::has_mac_address,
                       &DataRequest::Request::mutable_mac_address,
                       &MacAddress::mac_address, MacAddressToYangString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortMacAddressChangedEvent::GetMacAddress,
      MacAddressToYangString);
  auto register_functor = RegisterFunc<PortMacAddressChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/state/port-speed
void SetUpInterfacesInterfaceEthernetStatePortSpeed(uint64 node_id,
                                                    uint32 port_id,
                                                    TreeNode* node,
                                                    YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, tree, &DataResponse::port_speed,
      &DataResponse::has_port_speed, &DataRequest::Request::mutable_port_speed,
      &PortSpeed::speed_bps, ConvertSpeedBpsToString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortSpeedBpsChangedEvent::GetSpeedBps,
      ConvertSpeedBpsToString);
  auto register_functor = RegisterFunc<PortSpeedBpsChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/state/negotiated-port-speed
void SetUpInterfacesInterfaceEthernetStateNegotiatedPortSpeed(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, tree, &DataResponse::negotiated_port_speed,
      &DataResponse::has_negotiated_port_speed,
      &DataRequest::Request::mutable_negotiated_port_speed,
      &PortSpeed::speed_bps, ConvertSpeedBpsToString);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id,
      &PortNegotiatedSpeedBpsChangedEvent::GetNegotiatedSpeedBps,
      ConvertSpeedBpsToString);
  auto register_functor = RegisterFunc<PortNegotiatedSpeedBpsChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/state/forwarding-viable
void SetUpInterfacesInterfaceEthernetStateForwardingViability(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, tree, &DataResponse::forwarding_viability,
      &DataResponse::has_forwarding_viability,
      &DataRequest::Request::mutable_forwarding_viability,
      &ForwardingViability::state, ConvertTrunkMemberBlockStateToBool);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortForwardingViabilityChangedEvent::GetState,
      ConvertTrunkMemberBlockStateToBool);
  auto register_functor = RegisterFunc<PortForwardingViabilityChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/ethernet/state/auto-negotiate
void SetUpInterfacesInterfaceEthernetStateAutoNegotiate(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, tree, &DataResponse::autoneg_status,
      &DataResponse::has_autoneg_status,
      &DataRequest::Request::mutable_autoneg_status,
      &AutonegotiationStatus::state, IsPortAutonegEnabled);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortAutonegChangedEvent::GetState,
      IsPortAutonegEnabled);
  auto register_functor = RegisterFunc<PortAutonegChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

// A helper function that creates a functor that reads a counter from
// DataResponse::Counters proto buffer.
// 'func_ptr' points to protobuf field accessor method that reads the counter
// data from the DataResponse proto received from SwitchInterface, i.e.,
// "&DataResponse::PortCounters::message", where message field in
// DataResponse::Counters.
TreeNodeEventHandler GetPollCounterFunctor(
    uint64 node_id, uint32 port_id,
    ::google::protobuf::uint64 (PortCounters::*func_ptr)() const,
    YangParseTree* tree) {
  return [tree, node_id, port_id, func_ptr](const GnmiEvent& event,
                                            const ::gnmi::Path& path,
                                            GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    auto* request = req.add_requests()->mutable_port_counters();
    request->set_node_id(node_id);
    request->set_port_id(port_id);

    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    uint64 resp = 0;
    DataResponseWriter writer(
        [&resp, func_ptr](const DataResponse& in) -> bool {
          if (!in.has_port_counters()) return false;
          resp = (in.port_counters().*func_ptr)();
          return true;
        });
    // Query the switch. The returned status is ignored as there is no way to
    // notify the controller that something went wrong. The error is logged when
    // it is created.
    auto status = tree->GetSwitchInterface()->RetrieveValue(
        node_id, req, &writer, /* details= */ nullptr);
    return SendResponse(GetResponse(path, resp), stream);
  };
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-octets
void SetUpInterfacesInterfaceStateCountersInOctets(uint64 node_id,
                                                   uint32 port_id,
                                                   TreeNode* node,
                                                   YangParseTree* tree) {
  auto poll_functor =
      GetPollCounterFunctor(node_id, port_id, &PortCounters::in_octets, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInOctets);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/out-octets
void SetUpInterfacesInterfaceStateCountersOutOctets(uint64 node_id,
                                                    uint32 port_id,
                                                    TreeNode* node,
                                                    YangParseTree* tree) {
  auto poll_functor =
      GetPollCounterFunctor(node_id, port_id, &PortCounters::out_octets, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetOutOctets);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-unicast-pkts
void SetUpInterfacesInterfaceStateCountersInUnicastPkts(uint64 node_id,
                                                        uint32 port_id,
                                                        TreeNode* node,
                                                        YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::in_unicast_pkts, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInUnicastPkts);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/out-unicast-pkts
void SetUpInterfacesInterfaceStateCountersOutUnicastPkts(uint64 node_id,
                                                         uint32 port_id,
                                                         TreeNode* node,
                                                         YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::out_unicast_pkts, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetOutUnicastPkts);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-broadcast-pkts
void SetUpInterfacesInterfaceStateCountersInBroadcastPkts(uint64 node_id,
                                                          uint32 port_id,
                                                          TreeNode* node,
                                                          YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::in_broadcast_pkts, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInBroadcastPkts);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/out-broadcast-pkts
void SetUpInterfacesInterfaceStateCountersOutBroadcastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::out_broadcast_pkts, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetOutBroadcastPkts);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-discards
void SetUpInterfacesInterfaceStateCountersInDiscards(uint64 node_id,
                                                     uint32 port_id,
                                                     TreeNode* node,
                                                     YangParseTree* tree) {
  auto poll_functor =
      GetPollCounterFunctor(node_id, port_id, &PortCounters::in_discards, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInDiscards);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/out-discards
void SetUpInterfacesInterfaceStateCountersOutDiscards(uint64 node_id,
                                                      uint32 port_id,
                                                      TreeNode* node,
                                                      YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(node_id, port_id,
                                            &PortCounters::out_discards, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetOutDiscards);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-unknown-protos
void SetUpInterfacesInterfaceStateCountersInUnknownProtos(uint64 node_id,
                                                          uint32 port_id,
                                                          TreeNode* node,
                                                          YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::in_unknown_protos, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInUnknownProtos);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-multicast-pkts
void SetUpInterfacesInterfaceStateCountersInMulticastPkts(uint64 node_id,
                                                          uint32 port_id,
                                                          TreeNode* node,
                                                          YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::in_multicast_pkts, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInMulticastPkts);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-errors
void SetUpInterfacesInterfaceStateCountersInErrors(uint64 node_id,
                                                   uint32 port_id,
                                                   TreeNode* node,
                                                   YangParseTree* tree) {
  auto poll_functor =
      GetPollCounterFunctor(node_id, port_id, &PortCounters::in_errors, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInErrors);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/out-errors
void SetUpInterfacesInterfaceStateCountersOutErrors(uint64 node_id,
                                                    uint32 port_id,
                                                    TreeNode* node,
                                                    YangParseTree* tree) {
  auto poll_functor =
      GetPollCounterFunctor(node_id, port_id, &PortCounters::out_errors, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetOutErrors);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/in-fcs-errors
void SetUpInterfacesInterfaceStateCountersInFcsErrors(uint64 node_id,
                                                      uint32 port_id,
                                                      TreeNode* node,
                                                      YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(node_id, port_id,
                                            &PortCounters::in_fcs_errors, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetInFcsErrors);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /interfaces/interface[name=<name>]/state/counters/out-multicast-pkts
void SetUpInterfacesInterfaceStateCountersOutMulticastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetPollCounterFunctor(
      node_id, port_id, &PortCounters::out_multicast_pkts, tree);
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, &PortCountersChangedEvent::GetOutMulticastPkts);
  auto register_functor = RegisterFunc<PortCountersChangedEvent>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(unknown): remove/update this functor once the support for reading
  // counters is implemented.
  node->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/memory-error
void SetUpComponentsComponentChassisAlarmsMemoryError(TreeNode* node,
                                                      YangParseTree* tree) {
  auto register_functor = RegisterFunc<MemoryErrorAlarm>();
  node->SetOnChangeRegistration(register_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/memory-error/status
void SetUpComponentsComponentChassisAlarmsMemoryErrorStatus(
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      tree, &DataResponse::memory_error_alarm,
      &DataResponse::has_memory_error_alarm,
      &DataRequest::Request::mutable_memory_error_alarm, &Alarm::status);
  auto on_change_functor = GetOnChangeFunctor(&MemoryErrorAlarm::GetStatus);
  auto register_functor = RegisterFunc<MemoryErrorAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/memory-error/time-created
void SetUpComponentsComponentChassisAlarmsMemoryErrorTimeCreated(
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      tree, &DataResponse::memory_error_alarm,
      &DataResponse::has_memory_error_alarm,
      &DataRequest::Request::mutable_memory_error_alarm, &Alarm::time_created);
  auto on_change_functor =
      GetOnChangeFunctor(&MemoryErrorAlarm::GetTimeCreated);
  auto register_functor = RegisterFunc<MemoryErrorAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/memory-error/info
void SetUpComponentsComponentChassisAlarmsMemoryErrorInfo(TreeNode* node,
                                                          YangParseTree* tree) {
  // Regular method using a template cannot be used to get the OnPoll functor as
  // std::string fields are treated differently by the PROTO-to-C++ generator:
  // the getter returns "const std::string&" instead of "string" which leads to
  // the template compilation error.
  auto poll_functor = [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                             GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    *(req.add_requests()->mutable_memory_error_alarm()) =
        DataRequest::Request::Chassis();
    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    std::string resp{};
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_memory_error_alarm()) return false;
      resp = in.memory_error_alarm().description();
      return true;
    });
    // Query the switch. The returned status is ignored as there is no way to
    // notify the controller that something went wrong. The error is logged when
    // it is created.
    tree->GetSwitchInterface()
        ->RetrieveValue(/* node_id= */ 0, req, &writer, /* details= */ nullptr)
        .IgnoreError();
    return SendResponse(GetResponse(path, resp), stream);
  };

  auto on_change_functor = GetOnChangeFunctor(&MemoryErrorAlarm::GetInfo);
  auto register_functor = RegisterFunc<MemoryErrorAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/memory-error/severity
void SetUpComponentsComponentChassisAlarmsMemoryErrorSeverity(
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor =
      GetOnPollFunctor(tree, &DataResponse::memory_error_alarm,
                       &DataResponse::has_memory_error_alarm,
                       &DataRequest::Request::mutable_memory_error_alarm,
                       &Alarm::severity, ConvertAlarmSeverityToString);
  auto on_change_functor = GetOnChangeFunctor(&MemoryErrorAlarm::GetSeverity);
  auto register_functor = RegisterFunc<MemoryErrorAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/flow-programming-exception
void SetUpComponentsComponentChassisAlarmsFlowProgrammingException(
    TreeNode* node, YangParseTree* tree) {
  auto register_functor = RegisterFunc<FlowProgrammingExceptionAlarm>();
  node->SetOnChangeRegistration(register_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/
//     flow-programming-exception/status
void SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionStatus(
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      tree, &DataResponse::flow_programming_exception_alarm,
      &DataResponse::has_flow_programming_exception_alarm,
      &DataRequest::Request::mutable_flow_programming_exception_alarm,
      &Alarm::status);
  auto on_change_functor =
      GetOnChangeFunctor(&FlowProgrammingExceptionAlarm::GetStatus);
  auto register_functor = RegisterFunc<FlowProgrammingExceptionAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/
//     flow-programming-exception/time-created
void SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionTimeCreated(
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      tree, &DataResponse::flow_programming_exception_alarm,
      &DataResponse::has_flow_programming_exception_alarm,
      &DataRequest::Request::mutable_flow_programming_exception_alarm,
      &Alarm::time_created);
  auto on_change_functor =
      GetOnChangeFunctor(&FlowProgrammingExceptionAlarm::GetTimeCreated);
  auto register_functor = RegisterFunc<FlowProgrammingExceptionAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/
//     flow-programming-exception/info
void SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionInfo(
    TreeNode* node, YangParseTree* tree) {
  // Regular method using a template cannot be used to get the OnPoll functor as
  // std::string fields are treated differently by the PROTO-to-C++ generator:
  // the getter returns "const std::string&" instead of "string" which leads to
  // the template compilation error.
  auto poll_functor = [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                             GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    *(req.add_requests()->mutable_flow_programming_exception_alarm()) =
        DataRequest::Request::Chassis();
    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    std::string resp{};
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_flow_programming_exception_alarm()) return false;
      resp = in.flow_programming_exception_alarm().description();
      return true;
    });
    // Query the switch. The returned status is ignored as there is no way to
    // notify the controller that something went wrong. The error is logged when
    // it is created.
    tree->GetSwitchInterface()
        ->RetrieveValue(/* node_id= */ 0, req, &writer, /* details= */ nullptr)
        .IgnoreError();
    return SendResponse(GetResponse(path, resp), stream);
  };

  auto on_change_functor =
      GetOnChangeFunctor(&FlowProgrammingExceptionAlarm::GetInfo);
  auto register_functor = RegisterFunc<FlowProgrammingExceptionAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/chassis/alarms/
//     flow-programming-exception/severity
void SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionSeverity(
    TreeNode* node, YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      tree, &DataResponse::flow_programming_exception_alarm,
      &DataResponse::has_flow_programming_exception_alarm,
      &DataRequest::Request::mutable_flow_programming_exception_alarm,
      &Alarm::severity, ConvertAlarmSeverityToString);
  auto on_change_functor =
      GetOnChangeFunctor(&FlowProgrammingExceptionAlarm::GetSeverity);
  auto register_functor = RegisterFunc<FlowProgrammingExceptionAlarm>();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/transceiver/state/present
void SetUpComponentsComponentTransceiverStatePresent(
        TreeNode* node, YangParseTree* tree, uint64 node_id, uint32 port_id) {
  auto poll_functor = GetOnPollFunctor(
          node_id, port_id, tree, &DataResponse::front_panel_port_info,
          &DataResponse::has_front_panel_port_info,
          &DataRequest::Request::mutable_front_panel_port_info,
          &FrontPanelPortInfo::hw_state, ConvertHwStateToPresentString);
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/transceiver/state/serial-no
void SetUpComponentsComponentTransceiverStateSerialNo(
    TreeNode* node, YangParseTree* tree, uint64 node_id, uint32 port_id) {

  auto poll_functor =
      [tree, node_id, port_id](const GnmiEvent& event, const ::gnmi::Path& path,
                               GnmiSubscribeStream* stream) {
        // Create a data retrieval request.
        DataRequest req;
        auto* request = req.add_requests()->mutable_front_panel_port_info();
        request->set_node_id(node_id);
        request->set_port_id(port_id);

        // In-place definition of method retrieving data from generic response
        // and saving into 'resp' local variable.
        std::string resp{};
        DataResponseWriter writer([&resp](const DataResponse& in) {
          if (!in.has_front_panel_port_info()) return false;
          resp = in.front_panel_port_info().serial_number();
          return true;
        });
        // Query the switch. The returned status is ignored as there is no
        // way to notify the controller that something went wrong.
        // The error is logged when it is created.
        tree->GetSwitchInterface()
            ->RetrieveValue(node_id, req, &writer, /* details= */ nullptr)
            .IgnoreError();
        return SendResponse(GetResponse(path, resp), stream);
      };

  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/transceiver/state/vendor
void SetUpComponentsComponentTransceiverStateVendor(
    TreeNode* node, YangParseTree* tree, uint64 node_id, uint32 port_id) {

  auto poll_functor =
      [tree, node_id, port_id](const GnmiEvent& event,
                              const ::gnmi::Path& path,
                              GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    auto* request = req.add_requests()->mutable_front_panel_port_info();
    request->set_node_id(node_id);
    request->set_port_id(port_id);

    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    std::string resp{};
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_front_panel_port_info()) return false;
      resp = in.front_panel_port_info().vendor_name();
      return true;
    });
    // Query the switch. The returned status is ignored as there is no way to
    // notify the controller that something went wrong. The error is
    // logged when it is created.
    tree->GetSwitchInterface()
        ->RetrieveValue(node_id, req, &writer, /* details= */ nullptr)
        .IgnoreError();
    return SendResponse(GetResponse(path, resp), stream);
  };

  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/transceiver/state/vendor-part
void SetUpComponentsComponentTransceiverStateVendorPart(
    TreeNode* node, YangParseTree* tree, uint64 node_id, uint32 port_id) {
  auto poll_functor =
      [tree, node_id, port_id](const GnmiEvent& event,
                                const ::gnmi::Path& path,
                                GnmiSubscribeStream* stream) {
        // Create a data retrieval request.
        DataRequest req;
        auto* request = req.add_requests()->mutable_front_panel_port_info();
        request->set_node_id(node_id);
        request->set_port_id(port_id);

        // In-place definition of method retrieving data from generic response
        // and saving into 'resp' local variable.
        std::string resp{};
        DataResponseWriter writer([&resp](const DataResponse& in) {
          if (!in.has_front_panel_port_info()) return false;
          resp = in.front_panel_port_info().part_number();
          return true;
        });
        // Query the switch. The returned status is ignored as there is no
        // way to notify the controller that something went wrong.
        // The error is logged when it is created.
        tree->GetSwitchInterface()
            ->RetrieveValue(node_id, req, &writer, /* details= */ nullptr)
            .IgnoreError();
        return SendResponse(GetResponse(path, resp), stream);
      };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name>]/transceiver/state/form-factor
void SetUpComponentsComponentTransceiverStateFormFactor(
    TreeNode* node, YangParseTree* tree, uint64 node_id, uint32 port_id) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, tree, &DataResponse::front_panel_port_info,
      &DataResponse::has_front_panel_port_info,
      &DataRequest::Request::mutable_front_panel_port_info,
      &FrontPanelPortInfo::media_type, ConvertMediaTypeToString);
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/interfaces/interface[name=<name>]
//                    /output/queues/queue[name=<name>]/state/name
void SetUpQosInterfacesInterfaceOutputQueuesQueueStateName(
    const std::string& name, TreeNode* node) {
  auto poll_functor = [name](const GnmiEvent& event, const ::gnmi::Path& path,
                             GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, name), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/interfaces/interface[name=<name>]
//                    /output/queues/queue[name=<name>]/state/id
void SetUpQosInterfacesInterfaceOutputQueuesQueueStateId(uint64 node_id,
                                                         uint32 port_id,
                                                         uint32 queue_id,
                                                         TreeNode* node,
                                                         YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, queue_id, tree, &DataResponse::port_qos_counters,
      &DataResponse::has_port_qos_counters,
      &DataRequest::Request::mutable_port_qos_counters,
      &PortQosCounters::queue_id);
  auto register_functor = RegisterFunc<PortQosCountersChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, queue_id, &PortQosCountersChangedEvent::GetQueueId);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/interfaces/interface[name=<name>]
//                    /output/queues/queue[name=<name>]/state/transmit-pkts
void SetUpQosInterfacesInterfaceOutputQueuesQueueStateTransmitPkts(
    uint64 node_id, uint32 port_id, uint32 queue_id, TreeNode* node,
    YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, queue_id, tree, &DataResponse::port_qos_counters,
      &DataResponse::has_port_qos_counters,
      &DataRequest::Request::mutable_port_qos_counters,
      &PortQosCounters::out_pkts);
  auto register_functor = RegisterFunc<PortQosCountersChangedEvent>();
  auto on_change_functor =
      GetOnChangeFunctor(node_id, port_id, queue_id,
                         &PortQosCountersChangedEvent::GetTransmitPkts);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/interfaces/interface[name=<name>]
//                    /output/queues/queue[name=<name>]/state/transmit-octets
void SetUpQosInterfacesInterfaceOutputQueuesQueueStateTransmitOctets(
    uint64 node_id, uint32 port_id, uint32 queue_id, TreeNode* node,
    YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, queue_id, tree, &DataResponse::port_qos_counters,
      &DataResponse::has_port_qos_counters,
      &DataRequest::Request::mutable_port_qos_counters,
      &PortQosCounters::out_octets);
  auto register_functor = RegisterFunc<PortQosCountersChangedEvent>();
  auto on_change_functor =
      GetOnChangeFunctor(node_id, port_id, queue_id,
                         &PortQosCountersChangedEvent::GetTransmitOctets);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/interfaces/interface[name=<name>]
//                    /output/queues/queue[name=<name>]/state/dropped-pkts
void SetUpQosInterfacesInterfaceOutputQueuesQueueStateDroppedPkts(
    uint64 node_id, uint32 port_id, uint32 queue_id, TreeNode* node,
    YangParseTree* tree) {
  auto poll_functor = GetOnPollFunctor(
      node_id, port_id, queue_id, tree, &DataResponse::port_qos_counters,
      &DataResponse::has_port_qos_counters,
      &DataRequest::Request::mutable_port_qos_counters,
      &PortQosCounters::out_dropped_pkts);
  auto register_functor = RegisterFunc<PortQosCountersChangedEvent>();
  auto on_change_functor = GetOnChangeFunctor(
      node_id, port_id, queue_id, &PortQosCountersChangedEvent::GetDroppedPkts);
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeRegistration(register_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/queues/queue[name=<name>]/config/id
void SetUpQusQueuesQueueConfigId(uint32 queue_id, TreeNode* node,
                                 YangParseTree* tree) {
  auto poll_functor = [queue_id](const GnmiEvent& event,
                                 const ::gnmi::Path& path,
                                 GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when
    // it was configured!
    return SendResponse(GetResponse(path, queue_id), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /qos/queues/queue[name=<name>]/state/id
void SetUpQusQueuesQueueStateId(uint32 queue_id, TreeNode* node,
                                YangParseTree* tree) {
  auto poll_functor = [queue_id](const GnmiEvent& event,
                                 const ::gnmi::Path& path,
                                 GnmiSubscribeStream* stream) {
    // This leaf represents configuration data. Return what was known when
    // it was configured!
    return SendResponse(GetResponse(path, queue_id), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnTimerHandler(poll_functor)
      ->SetOnPollHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /debug/nodes/node[name=<name>]/packet-io/debug-string
void SetUpDebugNodesNodePacketIoDebugString(uint64 node_id, TreeNode* node,
                                            YangParseTree* tree) {
  // Regular method using a template cannot be used to get the OnPoll functor as
  // std::string fields are treated differently by the PROTO-to-C++ generator:
  // the getter returns "const std::string&" instead of "string" which leads to
  // the template compilation error.
  auto poll_functor = [node_id, tree](const GnmiEvent& event,
                                      const ::gnmi::Path& path,
                                      GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    auto* request = req.add_requests()->mutable_node_packetio_debug_info();
    request->set_node_id(node_id);
    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    std::string resp{};
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_node_packetio_debug_info()) return false;
      resp = in.node_packetio_debug_info().debug_string();
      return true;
    });
    // Query the switch. The returned status is ignored as there is no way to
    // notify the controller that something went wrong. The error is logged when
    // it is created.
    tree->GetSwitchInterface()
        ->RetrieveValue(node_id, req, &writer, /* details= */ nullptr)
        .IgnoreError();
    return SendResponse(GetResponse(path, resp), stream);
  };
  node->SetOnTimerHandler(poll_functor)->SetOnPollHandler(poll_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name-of-component>]/integrated-circuit/config/node-id
void SetUpComponentsComponentIntegratedCircuitConfigNodeId(
    uint64 node_id, TreeNode* node, YangParseTree* tree) {
  auto poll_functor = [node_id](const GnmiEvent& event,
                                const ::gnmi::Path& path,
                                GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, node_id), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

////////////////////////////////////////////////////////////////////////////////
// /components/component[name=<name-of-component>]/integrated-circuit/state/node-id
void SetUpComponentsComponentIntegratedCircuitStateNodeId(uint64 node_id,
                                                          TreeNode* node,
                                                          YangParseTree* tree) {
  auto poll_functor = [node_id](const GnmiEvent& event,
                                const ::gnmi::Path& path,
                                GnmiSubscribeStream* stream) {
    return SendResponse(GetResponse(path, node_id), stream);
  };
  auto on_change_functor = UnsupportedFunc();
  node->SetOnPollHandler(poll_functor)
      ->SetOnTimerHandler(poll_functor)
      ->SetOnChangeHandler(on_change_functor);
}

}  // namespace

// Path of leafs created by this method are defined 'manualy' by analysing
// existing YANG model files. They are hard-coded and, as  the YANG language
// does not allow to express leaf's semantics, their mapping to code
// implementing their function is also done manually.
// TODO(b/70300012): Implement a tool that will help to generate this code.
TreeNode* YangParseTreePaths::AddSubtreeInterface(
    const std::string& name, uint64 node_id, uint32 port_id,
    const NodeConfigParams& node_config, YangParseTree* tree) {
  // No need to lock the mutex - it is locked by method calling this one.
  TreeNode* node = tree->AddNode(
      GetPath("interfaces")("interface", name)("state")("last-change")());
  SetUpInterfacesInterfaceStateLastChange(node);

  node = tree->AddNode(
      GetPath("interfaces")("interface", name)("state")("ifindex")());
  SetUpInterfacesInterfaceStateIfindex(port_id, node);

  node = tree->AddNode(
      GetPath("interfaces")("interface", name)("state")("name")());
  SetUpInterfacesInterfaceStateName(name, node);

  node = tree->AddNode(
      GetPath("interfaces")("interface", name)("state")("oper-status")());
  SetUpInterfacesInterfaceStateOperStatus(node_id, port_id, node, tree);

  node = tree->AddNode(
      GetPath("interfaces")("interface", name)("state")("admin-status")());
  SetUpInterfacesInterfaceStateAdminStatus(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("hardware-port")());
  SetUpInterfacesInterfaceStateHardwarePort(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("ethernet")("state")("port-speed")());
  SetUpInterfacesInterfaceEthernetStatePortSpeed(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("ethernet")("state")("negotiated-port-speed")());
  SetUpInterfacesInterfaceEthernetStateNegotiatedPortSpeed(node_id, port_id,
                                                           node, tree);

  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(tmadejski) remove/update this functor once the support for reading
  // counters is implemented.
  tree->AddNode(GetPath("interfaces")("interface", name)("state")("counters")())
      ->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("in-octets")());
  SetUpInterfacesInterfaceStateCountersInOctets(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("out-octets")());
  SetUpInterfacesInterfaceStateCountersOutOctets(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("in-unicast-pkts")());
  SetUpInterfacesInterfaceStateCountersInUnicastPkts(node_id, port_id, node,
                                                     tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("out-unicast-pkts")());
  SetUpInterfacesInterfaceStateCountersOutUnicastPkts(node_id, port_id, node,
                                                      tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("in-broadcast-pkts")());
  SetUpInterfacesInterfaceStateCountersInBroadcastPkts(node_id, port_id, node,
                                                       tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("out-broadcast-pkts")());
  SetUpInterfacesInterfaceStateCountersOutBroadcastPkts(node_id, port_id, node,
                                                        tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("in-multicast-pkts")());
  SetUpInterfacesInterfaceStateCountersInMulticastPkts(node_id, port_id, node,
                                                       tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("out-multicast-pkts")());
  SetUpInterfacesInterfaceStateCountersOutMulticastPkts(node_id, port_id, node,
                                                        tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("in-discards")());
  SetUpInterfacesInterfaceStateCountersInDiscards(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("out-discards")());
  SetUpInterfacesInterfaceStateCountersOutDiscards(node_id, port_id, node,
                                                   tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("in-unknown-protos")());
  SetUpInterfacesInterfaceStateCountersInUnknownProtos(node_id, port_id, node,
                                                       tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("in-errors")());
  SetUpInterfacesInterfaceStateCountersInErrors(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("out-errors")());
  SetUpInterfacesInterfaceStateCountersOutErrors(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("state")("counters")("in-fcs-errors")());
  SetUpInterfacesInterfaceStateCountersInFcsErrors(node_id, port_id, node,
                                                   tree);

  node = tree->AddNode(GetPath("lacp")("interfaces")(
      "interface", name)("state")("system-priority")());
  SetUpLacpInterfacesInterfaceStateSystemPriority(node_id, port_id, node, tree);

  node = tree->AddNode(
      GetPath("interfaces")("interface", name)("config")("health-indicator")());
  // TODO(tmadejski): Fix this value once common.proto has corresponding field.
  SetUpInterfacesInterfaceConfigHealthIndicator("GOOD", node_id, port_id, node,
                                                tree);

  node = tree->AddNode(
      GetPath("interfaces")("interface", name)("state")("health-indicator")());
  SetUpInterfacesInterfaceStateHealthIndicator(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("ethernet")("config")("forwarding-viable")());
  // TODO(tmadejski): Fix this value once common.proto has corresponding field.
  SetUpInterfacesInterfaceEthernetConfigForwardingViability(node_id, port_id,
      /* forwarding-viable */ true, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("ethernet")("state")("forwarding-viable")());
  SetUpInterfacesInterfaceEthernetStateForwardingViability(node_id, port_id,
                                                           node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("ethernet")("state")("auto-negotiate")());
  SetUpInterfacesInterfaceEthernetStateAutoNegotiate(node_id, port_id,
                                                      node, tree);

  absl::flat_hash_map<uint32, uint32> internal_priority_to_q_num;
  absl::flat_hash_map<uint32, TrafficClass> q_num_to_trafic_class;
  for (const auto& e : node_config.qos_config().cosq_mapping()) {
    internal_priority_to_q_num[e.internal_priority()] = e.q_num();
  }
  for (const auto& e : node_config.qos_config().traffic_class_mapping()) {
    uint32* q_num =
        gtl::FindOrNull(internal_priority_to_q_num, e.internal_priority());
    if (q_num != nullptr) {
      gtl::InsertIfNotPresent(&q_num_to_trafic_class, *q_num,
                              e.traffic_class());
    }
  }

  for (const auto& e : q_num_to_trafic_class) {
    // TODO(unknown): Use consistent names for queue numbers. Either q_num
    // or q_id or queue_id.
    uint32 queue_id = e.first;
    std::string queue_name = TrafficClass_Name(e.second);

    // Add output-qos-related leafs.
    node = tree->AddNode(GetPath("qos")("interfaces")("interface", name)(
        "output")("queues")("queue", queue_name)("state")("name")());
    SetUpQosInterfacesInterfaceOutputQueuesQueueStateName(queue_name, node);

    node = tree->AddNode(GetPath("qos")("interfaces")("interface", name)(
        "output")("queues")("queue", queue_name)("state")("id")());
    SetUpQosInterfacesInterfaceOutputQueuesQueueStateId(node_id, port_id,
                                                        queue_id, node, tree);

    node = tree->AddNode(GetPath("qos")("interfaces")("interface", name)(
        "output")("queues")("queue", queue_name)("state")("transmit-pkts")());
    SetUpQosInterfacesInterfaceOutputQueuesQueueStateTransmitPkts(
        node_id, port_id, queue_id, node, tree);

    node = tree->AddNode(GetPath("qos")("interfaces")("interface", name)(
        "output")("queues")("queue", queue_name)("state")("transmit-octets")());
    SetUpQosInterfacesInterfaceOutputQueuesQueueStateTransmitOctets(
        node_id, port_id, queue_id, node, tree);

    node = tree->AddNode(GetPath("qos")("interfaces")("interface", name)(
        "output")("queues")("queue", queue_name)("state")("dropped-pkts")());
    SetUpQosInterfacesInterfaceOutputQueuesQueueStateDroppedPkts(
        node_id, port_id, queue_id, node, tree);

    node = tree->AddNode(GetPath("qos")("queues")("queue", queue_name)(
           "config")("id")());
    SetUpQusQueuesQueueConfigId(queue_id, node, tree);

    node = tree->AddNode(GetPath("qos")("queues")("queue", queue_name)(
           "state")("id")());
    SetUpQusQueuesQueueStateId(queue_id, node, tree);
  }

  return node;
}

void YangParseTreePaths::AddSubtreeInterfaceFromTrunk(
    const std::string& name, uint64 node_id, uint32 port_id,
    const NodeConfigParams& node_config, YangParseTree* tree) {
  AddSubtreeInterface(name, node_id, port_id, node_config, tree);
}

void YangParseTreePaths::AddSubtreeInterfaceFromSingleton(
    const SingletonPort& singleton, const NodeConfigParams& node_config,
    YangParseTree* tree) {
  const std::string& name = singleton.name();
  uint64 node_id = singleton.node();
  uint32 port_id = singleton.id();
  TreeNode* node =
      AddSubtreeInterface(name, node_id, port_id, node_config, tree);

  node = tree->AddNode(GetPath("lacp")("interfaces")(
      "interface", name)("state")("system-id-mac")());
  SetUpLacpInterfacesInterfaceStateSystemIdMac(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("ethernet")("state")("mac-address")());
  SetUpInterfacesInterfaceEthernetStateMacAddress(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("ethernet")("config")("port-speed")());
  SetUpInterfacesInterfaceEthernetConfigPortSpeed(node_id, port_id,
                                                  singleton.speed_bps(), node,
                                                  tree);
  bool port_auto_neg_enabled = false;
  bool port_enabled = false;
  uint64 mac_address = 0;
  if (singleton.has_config_params()) {
    port_auto_neg_enabled =
        IsPortAutonegEnabled(singleton.config_params().autoneg());
    port_enabled = IsAdminStateEnabled(singleton.config_params().admin_state());
    mac_address = singleton.config_params().mac_address();
  }

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("ethernet")("config")("auto-negotiate")());
  SetUpInterfacesInterfaceEthernetConfigAutoNegotiate(
      node_id, port_id, port_auto_neg_enabled, node, tree);
  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("config")("enabled")());
  SetUpInterfacesInterfaceConfigEnabled(port_enabled, node_id, port_id, node,
                                        tree);

  node = tree->AddNode(GetPath("interfaces")(
      "interface", name)("ethernet")("config")("mac-address")());
  SetUpInterfacesInterfaceEthernetConfigMacAddress(node_id, port_id,
                                                   mac_address, node, tree);

  // Paths for transceiver
  node = tree->AddNode(GetPath("components")(
      "component", name)("transceiver")("state")("present")());
  SetUpComponentsComponentTransceiverStatePresent(node, tree, node_id, port_id);
  node = tree->AddNode(GetPath("components")(
      "component", name)("transceiver")("state")("serial-no")());
  SetUpComponentsComponentTransceiverStateSerialNo(node, tree, node_id,
                                                   port_id);

  node = tree->AddNode(GetPath("components")(
      "component", name)("transceiver")("state")("vendor")());
  SetUpComponentsComponentTransceiverStateVendor(node, tree, node_id, port_id);

  node = tree->AddNode(GetPath("components")(
      "component", name)("transceiver")("state")("vendor-part")());
  SetUpComponentsComponentTransceiverStateVendorPart(node, tree, node_id,
                                                     port_id);

  node = tree->AddNode(GetPath("components")(
      "component", name)("transceiver")("state")("form-factor")());
  SetUpComponentsComponentTransceiverStateFormFactor(node, tree, node_id,
                                                     port_id);
}

void YangParseTreePaths::AddSubtreeNode(const Node& node, YangParseTree* tree) {
  // No need to lock the mutex - it is locked by method calling this one.
  TreeNode* tree_node = tree->AddNode(GetPath("debug")("nodes")(
      "node", node.name())("packet-io")("debug-string")());
  SetUpDebugNodesNodePacketIoDebugString(node.id(), tree_node, tree);
  tree_node = tree->AddNode(GetPath("components")("component", node.name())
      ("integrated-circuit")("config")("node-id")());
  SetUpComponentsComponentIntegratedCircuitConfigNodeId(node.id(),
                                                        tree_node, tree);

  tree_node = tree->AddNode(GetPath("components")("component", node.name())
      ("integrated-circuit")("state")("node-id")());
  SetUpComponentsComponentIntegratedCircuitStateNodeId(node.id(),
                                                       tree_node, tree);
}

void YangParseTreePaths::AddSubtreeChassis(const Chassis& chassis,
                                           YangParseTree* tree) {
  const std::string& name = chassis.name();
  TreeNode* node = tree->AddNode(GetPath("components")(
      "component", name)("chassis")("alarms")("memory-error")());
  SetUpComponentsComponentChassisAlarmsMemoryError(node, tree);
  node = tree->AddNode(GetPath("components")(
      "component", name)("chassis")("alarms")("memory-error")("status")());
  SetUpComponentsComponentChassisAlarmsMemoryErrorStatus(node, tree);
  node = tree->AddNode(GetPath("components")("component", name)("chassis")(
      "alarms")("memory-error")("time-created")());
  SetUpComponentsComponentChassisAlarmsMemoryErrorTimeCreated(node, tree);
  node = tree->AddNode(GetPath("components")(
      "component", name)("chassis")("alarms")("memory-error")("info")());
  SetUpComponentsComponentChassisAlarmsMemoryErrorInfo(node, tree);
  node = tree->AddNode(GetPath("components")(
      "component", name)("chassis")("alarms")("memory-error")("severity")());
  SetUpComponentsComponentChassisAlarmsMemoryErrorSeverity(node, tree);

  node = tree->AddNode(GetPath("components")(
      "component", name)("chassis")("alarms")("flow-programming-exception")());
  SetUpComponentsComponentChassisAlarmsFlowProgrammingException(node, tree);
  node = tree->AddNode(GetPath("components")("component", name)("chassis")(
      "alarms")("flow-programming-exception")("status")());
  SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionStatus(node,
                                                                      tree);
  node = tree->AddNode(GetPath("components")("component", name)("chassis")(
      "alarms")("flow-programming-exception")("time-created")());
  SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionTimeCreated(
      node, tree);
  node = tree->AddNode(GetPath("components")("component", name)("chassis")(
      "alarms")("flow-programming-exception")("info")());
  SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionInfo(node, tree);
  node = tree->AddNode(GetPath("components")("component", name)("chassis")(
      "alarms")("flow-programming-exception")("severity")());
  SetUpComponentsComponentChassisAlarmsFlowProgrammingExceptionSeverity(node,
                                                                        tree);
}

void YangParseTreePaths::AddSubtreeAllInterfaces(YangParseTree* tree) {
  // Add support for "/interfaces/interface[name=*]/state/ifindex".
  tree->AddNode(GetPath("interfaces")("interface", "*")("state")("ifindex")())
      ->SetOnChangeRegistration(
          [tree](const EventHandlerRecordPtr& record)
              EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
                // Subscribing to a wildcard node means that all matching nodes
                // have to be registered for received events.
                auto status = tree->PerformActionForAllNonWildcardNodes(
                    GetPath("interfaces")("interface")(),
                    GetPath("state")("ifindex")(),
                    [&record](const TreeNode& node) {
                      return node.DoOnChangeRegistration(record);
                    });
                return status;
              })
      ->SetOnChangeHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream) { return ::util::OkStatus(); })
      ->SetOnPollHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream)
              EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
                // Polling a wildcard node means that all matching nodes have to
                // be polled.
                auto status = tree->PerformActionForAllNonWildcardNodes(
                    GetPath("interfaces")("interface")(),
                    GetPath("state")("ifindex")(),
                    [&event, &stream](const TreeNode& leaf) {
                      return (leaf.GetOnPollHandler())(event, stream);
                    });
                // Notify the client that all nodes have been processed.
                APPEND_STATUS_IF_ERROR(
                    status, YangParseTreePaths::SendEndOfSeriesMessage(stream));
                return status;
              });
  // Add support for "/interfaces/interface[name=*]/state/name".
  tree->AddNode(GetPath("interfaces")("interface", "*")("state")("name")())
      ->SetOnChangeRegistration(
          [tree](const EventHandlerRecordPtr& record)
              EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
                // Subscribing to a wildcard node means that all matching nodes
                // have to be registered for received events.
                auto status = tree->PerformActionForAllNonWildcardNodes(
                    GetPath("interfaces")("interface")(),
                    GetPath("state")("name")(),
                    [&record](const TreeNode& node) {
                      return node.DoOnChangeRegistration(record);
                    });
                return status;
              })
      ->SetOnChangeHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream) { return ::util::OkStatus(); })
      ->SetOnPollHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream)
              EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
                // Polling a wildcard node means that all matching nodes have to
                // be polled.
                auto status = tree->PerformActionForAllNonWildcardNodes(
                    GetPath("interfaces")("interface")(),
                    GetPath("state")("name")(),
                    [&event, &stream](const TreeNode& leaf) {
                      return (leaf.GetOnPollHandler())(event, stream);
                    });
                // Notify the client that all nodes have been processed.
                APPEND_STATUS_IF_ERROR(
                    status, YangParseTreePaths::SendEndOfSeriesMessage(stream));
                return status;
              });

  auto interfaces_on_chage_reg = [tree](const EventHandlerRecordPtr& record)
  EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
    // Subscribing to a wildcard node means that all matching nodes
    // have to be registered for received events.
    auto status = tree->PerformActionForAllNonWildcardNodes(
        GetPath("interfaces")("interface")(), gnmi::Path(),
        [&record](const TreeNode& node) {
          return node.DoOnChangeRegistration(record);
        });
    return status;
  };  // NOLINT(readability/braces)

  auto interfaces_on_poll = [tree](const GnmiEvent& event,
                                   const ::gnmi::Path& path,
                                   GnmiSubscribeStream* stream)
  EXCLUSIVE_LOCKS_REQUIRED(tree->root_access_lock_) {
    // Polling a wildcard node means that all matching nodes have to
    // be polled.
    auto status = tree->PerformActionForAllNonWildcardNodes(
        GetPath("interfaces")("interface")(), gnmi::Path(),
        [&event, &stream](const TreeNode& node) {
          return (node.GetOnPollHandler())(event, stream);
        });
    // Notify the client that all nodes have been processed.
    APPEND_STATUS_IF_ERROR(
        status, YangParseTreePaths::SendEndOfSeriesMessage(stream));
    return status;
  };  // NOLINT(readability/braces)

  // Add support for "/interfaces/interface/...".
  tree->AddNode(GetPath("interfaces")("interface")("...")())
      ->SetOnChangeRegistration(interfaces_on_chage_reg)
      ->SetOnChangeHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream) { return ::util::OkStatus(); })
      ->SetOnPollHandler(interfaces_on_poll);

  // Add support for "/interfaces/interface/*".
  tree->AddNode(GetPath("interfaces")("interface")("*")())
      ->SetOnChangeRegistration(interfaces_on_chage_reg)
      ->SetOnChangeHandler(
          [tree](const GnmiEvent& event, const ::gnmi::Path& path,
                 GnmiSubscribeStream* stream) { return ::util::OkStatus(); })
      ->SetOnPollHandler(interfaces_on_poll);
}

void YangParseTreePaths::AddRoot(YangParseTree* tree) {
  // Add support for "/"
  SetUpRoot(tree->AddNode(GetPath()()), tree);
}

// A helper method that handles sending a message that marks the end of series
// of update messages.
::util::Status YangParseTreePaths::SendEndOfSeriesMessage(
    GnmiSubscribeStream* stream) {
  // Notify the client that all nodes have been processed.
  ::gnmi::SubscribeResponse resp;
  resp.set_sync_response(true);
  return SendResponse(resp, stream);
}

}  // namespace hal
}  // namespace stratum
