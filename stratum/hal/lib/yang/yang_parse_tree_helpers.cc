// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "yang_parse_tree_helpers.h"

#include "absl/time/clock.h"

namespace stratum {
namespace hal {
namespace yang {
namespace helpers {

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

// Specialization for '::gnmi::Decimal64'.
::gnmi::SubscribeResponse GetResponse(const ::gnmi::Path& path,
                                      const ::gnmi::Decimal64& contents) {
  ::gnmi::SubscribeResponse resp = GetResponse(path);
  *resp.mutable_update()
       ->mutable_update(0)
       ->mutable_val()
       ->mutable_decimal_val() = contents;
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

}  // namespace helpers
}  // namespace yang
}  // namespace hal
}  // namespace stratum
