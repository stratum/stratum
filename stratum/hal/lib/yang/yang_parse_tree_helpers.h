// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_YANG_YANG_PARSE_TREE_HELPERS_H_
#define STRATUM_HAL_LIB_YANG_YANG_PARSE_TREE_HELPERS_H_

#include "gnmi/gnmi.pb.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/yang/yang_parse_tree.h"

namespace stratum {
namespace hal {
namespace yang {
namespace helpers {

// A helper method that prepares the gNMI message.
::gnmi::SubscribeResponse GetResponse(const ::gnmi::Path& path);

// Specialization for 'const char*'.
::gnmi::SubscribeResponse GetResponse(const ::gnmi::Path& path,
                                      const char* contents);

// Specialization for 'const std::string&'.
::gnmi::SubscribeResponse GetResponse(const ::gnmi::Path& path,
                                      const std::string& contents);

// Specialization for 'bool'.
::gnmi::SubscribeResponse GetResponse(const ::gnmi::Path& path,
                                      const bool contents);

// Specialization for '::gnmi::Decimal64'.
::gnmi::SubscribeResponse GetResponse(const ::gnmi::Path& path,
                                      const ::gnmi::Decimal64& contents);

// A helper method that handles writing a response into the output stream.
::util::Status SendResponse(const ::gnmi::SubscribeResponse& resp,
                            GnmiSubscribeStream* stream);

// A helper method that returns a dummy functor that returns 'not supported yet'
// string.
TreeNodeEventHandler UnsupportedFunc();

// A helper method returning TRUE if the event passed to the handler is a timer.
bool IsTimer(const GnmiEvent& event);

// A helper method returning TRUE if the event passed to the handler is a poll
// request.
bool IsPoll(const GnmiEvent& event);

// A helper method returning TRUE if the event passed to the handler is a
// notification about a config being pushed..
bool HasConfigBeenPushed(const GnmiEvent& event);

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

// Optical Port-specific version. Extra parameters needed:
// - module index ('module')
// - network interface index ('network_interface')
template <typename U, typename V>
::util::Status SetValue(int32 module, int32 network_interface,
                        YangParseTree* tree,
                        void (OpticalTransceiverInfo::*set_field_func)(U),
                        const V& value) {
  // Create a set request.
  SetRequest req;
  auto* request = req.add_requests()->mutable_optical_network_interface();
  request->set_module(module);
  request->set_network_interface(network_interface);
  (request->mutable_optical_transceiver_info()->*set_field_func)(value);
  // Note that the "node_id" parameter won't be used in this case so we put
  // a default integer value 0 here.
  std::vector<::util::Status> details;
  tree->GetSwitchInterface()
      ->SetValue(/*node_id*/ 0, req, &details)
      .IgnoreError();
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

// Port-specific version.
// Can be used for two-level nested messages (DataResponse::T::V).
template <typename T, typename U, typename V>
U GetValue(
    uint64 node_id, uint32 port_id, YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::Port* (
        DataRequest::Request::*data_request_get_mutable_inner_message_func)(),
    bool (T::*inner_message_has_inner_message_func)() const,
    const V& (T::*inner_message_get_inner_message_func)() const,
    U (V::*inner_message_get_field_func)() const) {
  // Create a data retrieval request.
  DataRequest req;
  auto* request =
      (req.add_requests()->*data_request_get_mutable_inner_message_func)();
  request->set_node_id(node_id);
  request->set_port_id(port_id);
  // In-place definition of method retrieving data from generic response
  // and saving into 'resp' local variable.
  U resp{};
  // Writer for retrieving value
  DataResponseWriter writer([&resp, data_response_get_inner_message_func,
                             data_response_has_inner_message_func,
                             inner_message_has_inner_message_func,
                             inner_message_get_inner_message_func,
                             inner_message_get_field_func](
                                const DataResponse& in) {
    if (!(in.*data_response_has_inner_message_func)()) return false;
    auto inner_msg = (in.*data_response_get_inner_message_func)();
    if (!(inner_msg.*inner_message_has_inner_message_func)()) return false;
    auto inner_msg_field = (inner_msg.*inner_message_get_inner_message_func)();
    resp = (inner_msg_field.*inner_message_get_field_func)();
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

// Optical Port-specific version. Extra parameters needed:
// - module index ('module')
// - network interface index ('network_interface')
template <typename T, typename U>
TreeNodeEventHandler GetOnPollFunctor(
    int32 module, int32 network_interface, YangParseTree* tree,
    T (OpticalTransceiverInfo::*inner_message_get_field_func)() const,
    U (*process_func)(const T&)) {
  return [=](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    auto* request = req.add_requests()->mutable_optical_transceiver_info();
    request->set_module(module);
    request->set_network_interface(network_interface);
    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    OpticalTransceiverInfo resp{};
    // Writer for retrieving value
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_optical_transceiver_info()) return false;
      resp = in.optical_transceiver_info();
      return true;
    });
    // Query the switch. The returned status is ignored as there is no way to
    // notify the controller that something went wrong. The error is logged when
    // it is created.
    // Here we ignore the node_id since it is not valid in this case.
    tree->GetSwitchInterface()
        ->RetrieveValue(/*node_id*/ 0, req, &writer, /* details= */ nullptr)
        .IgnoreError();
    // Return the retrieved value.
    T value = (resp.*inner_message_get_field_func)();
    return SendResponse(GetResponse(path, (*process_func)(value)), stream);
  };
}

// Optical Port-specific version. Extra parameters needed:
// - module index ('module')
// - network interface index ('network_interface')
template <typename T, typename U, typename V>
TreeNodeEventHandler GetOnPollFunctor(
    int32 module, int32 network_interface, YangParseTree* tree,
    bool (OpticalTransceiverInfo::*has_inner_msg_func)() const,
    const T& (OpticalTransceiverInfo::*get_inner_msg_func)() const,
    U (T::*get_inner_field_func)() const, V (*process_field_func)(const U&)) {
  return [=](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    // Create a data retrieval request.
    DataRequest req;
    auto* request = req.add_requests()->mutable_optical_transceiver_info();
    request->set_module(module);
    request->set_network_interface(network_interface);
    // In-place definition of method retrieving data from generic response
    // and saving into 'resp' local variable.
    OpticalTransceiverInfo resp{};
    // Writer for retrieving value
    DataResponseWriter writer([&resp](const DataResponse& in) {
      if (!in.has_optical_transceiver_info()) return false;
      resp = in.optical_transceiver_info();
      return true;
    });
    // Query the switch. The returned status is ignored as there is no way to
    // notify the controller that something went wrong. The error is logged when
    // it is created.
    // Here we ignore the node_id since it is not valid in this case.
    tree->GetSwitchInterface()
        ->RetrieveValue(/*node_id*/ 0, req, &writer, /* details= */ nullptr)
        .IgnoreError();
    // Return the retrieved value. Note that we will return a default value if
    // the second level nest message does not exists.
    V value{};
    if ((resp.*has_inner_msg_func)()) {
      const T& inner_msg = (resp.*get_inner_msg_func)();
      const U& inner_field = (inner_msg.*get_inner_field_func)();
      value = process_field_func(inner_field);
    }
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

// Port-specific version.
// Can be used for two-level nested messages (DataResponse::T::U).
template <typename T, typename U, typename V>
TreeNodeEventHandler GetOnPollFunctor(
    uint64 node_id, uint32 port_id, YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::Port* (
        DataRequest::Request::*get_mutable_inner_message_func)(),
    bool (T::*inner_message_has_inner_message_func)() const,
    const U& (T::*inner_message_get_inner_message_func)() const,
    V (U::*inner_message_get_field_func)() const) {
  return [=](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    V value = GetValue(
        node_id, port_id, tree, data_response_get_inner_message_func,
        data_response_has_inner_message_func, get_mutable_inner_message_func,
        inner_message_has_inner_message_func,
        inner_message_get_inner_message_func, inner_message_get_field_func);
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

// Port-specific version.
// Can be used for two-level nested messages (DataResponse::T::U).
// We omit the cast from U to V and expect the same type.
template <typename T, typename U, typename V, typename W>
TreeNodeEventHandler GetOnPollFunctor(
    uint64 node_id, uint32 port_id, YangParseTree* tree,
    const T& (DataResponse::*data_response_get_inner_message_func)() const,
    bool (DataResponse::*data_response_has_inner_message_func)() const,
    DataRequest::Request::Port* (
        DataRequest::Request::*get_mutable_inner_message_func)(),
    bool (T::*inner_message_has_inner_message_func)() const,
    const U& (T::*inner_message_get_inner_message_func)() const,
    V (U::*inner_message_get_field_func)() const, W (*process_func)(const V&)) {
  return [=](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    V value = GetValue(
        node_id, port_id, tree, data_response_get_inner_message_func,
        data_response_has_inner_message_func, get_mutable_inner_message_func,
        inner_message_has_inner_message_func,
        inner_message_get_inner_message_func, inner_message_get_field_func);
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

// Optical Network Interface-specific version.
template <typename T, typename U, typename V>
TreeNodeEventHandler GetOnChangeFunctor(int32 module, int32 network_interface,
                                        U (T::*get_func_ptr)() const,
                                        V (*process_func)(const U&)) {
  return [=](const GnmiEvent& event, const ::gnmi::Path& path,
             GnmiSubscribeStream* stream) {
    // For now, we are interested in events of type T only!
    const T* change = dynamic_cast<const T*>(&event);
    if (change == nullptr || change->GetModule() != module ||
        change->GetNetworkInterface() != network_interface) {
      // This is not the event you are looking for...
      return ::util::OkStatus();
    }
    return SendResponse(
        GetResponse(path, (*process_func)((change->*get_func_ptr)())), stream);
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

}  // namespace helpers
}  // namespace yang
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_YANG_YANG_PARSE_TREE_HELPERS_H_
