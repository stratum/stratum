// Copyright 2018 Google LLC
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


#include "stratum/hal/lib/common/phaldb_service.h"

#include <functional>
#include <sstream>  // IWYU pragma: keep

#include "gflags/gflags.h"
#include "absl/strings/str_split.h"
#include "google/protobuf/any.pb.h"
#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "stratum/glue/gtl/cleanup.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/managed_attribute.h"
#include "re2/re2.h"

namespace stratum {
namespace hal {

PhalDBService::PhalDBService(OperationMode mode, 
                     PhalInterface* phal_interface,
                     AuthPolicyChecker* auth_policy_checker,
                     ErrorBuffer* error_buffer)
    : mode_(mode),
      auth_policy_checker_(ABSL_DIE_IF_NULL(auth_policy_checker)),
      error_buffer_(ABSL_DIE_IF_NULL(error_buffer)),
      phal_interface_(phal_interface) {
}

PhalDBService::~PhalDBService() {}

::util::Status PhalDBService::Setup(bool warmboot) {
  return ::util::OkStatus();
}

::util::Status PhalDBService::Teardown() {
  
  LOG(INFO) << "PhalDBService::Teardown";

  {
    absl::MutexLock l(&subscriber_thread_lock_);
    // Close Subscriber Channels.
    for (const auto& pair : subscriber_channels_) {
      pair.second->Close();
    }
    subscriber_channels_.clear();
  }

  return ::util::OkStatus();
}

namespace {

::grpc::Status ToGrpcStatus(const ::util::Status& status,
                            const std::vector<::util::Status>& details) {

  // We need to create a ::google::rpc::Status and populate it with all the
  // details, then convert it to ::grpc::Status.
  ::google::rpc::Status from;
  if (!status.ok()) {
    from.set_code(ToGoogleRpcCode(status.CanonicalCode()));
    from.set_message(status.error_message());
    // Add individual errors only when the top level error code is not OK.
    for (const auto& detail : details) {
      // Each individual detail is converted to another ::google::rpc::Status,
      // which is then serialized as one proto any in 'from' message above.
      ::stratum::hal::phal::Error error;
      if (!detail.ok()) {
        error.set_canonical_code(ToGoogleRpcCode(detail.CanonicalCode()));
        error.set_code(detail.error_code());
        error.set_message(detail.error_message());
      } else {
        error.set_code(::google::rpc::OK);
      }
      from.add_details()->PackFrom(error);
    }
  } else {
    from.set_code(::google::rpc::OK);
  }

  return ::grpc::Status(ToGrpcCode(from.code()), from.message(),
                        from.SerializeAsString());
}

// Parse PB Query string to Phal DB Path
::util::StatusOr<::stratum::hal::phal::Path> ParseQuery(
    const std::string& query) {

    ::stratum::hal::phal::Path path;

    std::vector<std::string> query_fields = absl::StrSplit(query, "/");
    bool use_terminal_group = false;
    if (query_fields[query_fields.size() - 1] == "") {
      use_terminal_group = true;  // Query ends with a '/'
      query_fields = {query_fields.begin(),
                      query_fields.begin() + query_fields.size() - 1};
    }

    for (const auto& query_field : query_fields) {
      CHECK_RETURN_IF_FALSE(query_field != "")
          << "Encountered unexpected empty query field.";
      RE2 field_regex(R"#((\w+)(\[(?:\d+|\@)\])?)#");
      RE2 bracket_regex(R"#(\[(\d+)\])#");

      ::stratum::hal::phal::PathEntry entry;

      std::string bracket_match;
      CHECK_RETURN_IF_FALSE(
          RE2::FullMatch(query_field, field_regex, &entry.name, &bracket_match))
          << "Could not parse query field: " << query_field;
      if (!bracket_match.empty()) {
        entry.indexed = true;
        if (!RE2::FullMatch(bracket_match, bracket_regex, &entry.index))
          entry.all = true;
      }
      path.push_back(entry);
    }

    path[path.size() - 1].terminal_group = use_terminal_group;

    return path;
}

// Convert from ProtoBuf Path to PhalDB Path
::util::StatusOr<::stratum::hal::phal::Path> ToPhalDBPath(
    ::stratum::hal::phal::PathQuery req_path) {

    ::stratum::hal::phal::Path path;

    // If no path entries return error
    if (req_path.entries_size() == 0) {
        RETURN_ERROR(ERR_INVALID_PARAM) << "No Path";
    }

    // Create Attribute DB Path
    ::stratum::hal::phal::PathEntry entry;
    for (const auto& ent : req_path.entries()) {
        entry.name = ent.name();
        entry.index = ent.index();
        entry.indexed = ent.indexed();
        entry.all = ent.all();
        entry.terminal_group = ent.terminal_group();
        path.push_back(entry);
    }

    return path;
}

}  // namespace

::grpc::Status PhalDBService::Get(::grpc::ServerContext* context,
    const ::stratum::hal::phal::GetRequest* req,
    ::stratum::hal::phal::GetResponse* resp) {

  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, PhalDBService, Get, context);

  ::util::Status status;
  ::util::StatusOr<::stratum::hal::phal::Path> result;

  // Convert to PhalDB Path
  switch (req->query_case()) {
    case ::stratum::hal::phal::GetRequest::kStr: {
        result = ParseQuery(req->str());
        break;
    }
    case ::stratum::hal::phal::GetRequest::kPath: {
        result = ToPhalDBPath(req->path());
        break;
    }
    default:
        return ::grpc::Status(
            ::grpc::StatusCode::INVALID_ARGUMENT,
            "Invalid query in Get request.");
  }

  if (!result.ok()) return ToGrpcStatus(result.status(), {});

  auto path = result.ConsumeValueOrDie();

  // Issue the get
  auto adapter = 
    absl::make_unique<::stratum::hal::phal::Adapter>(phal_interface_);
  auto phaldb_res = adapter->Get({path});
  if (phaldb_res.ok()) {
      auto phaldb_resp = std::move(phaldb_res.ConsumeValueOrDie());
      *resp->mutable_phal_db() = *phaldb_resp;
  }

  // Convert to grpc status and return
  return ToGrpcStatus(phaldb_res.status(), {});
}

::grpc::Status PhalDBService::Set(::grpc::ServerContext* context,
    const ::stratum::hal::phal::SetRequest* req,
    ::stratum::hal::phal::SetResponse* resp) {

  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, PhalDBService, Set, context);

  if (!req->updates_size()) return ::grpc::Status::OK;  // Nothing to do.

  ::util::Status status = ::util::OkStatus();
  std::vector<::util::Status> results = {};
  stratum::hal::phal::AttributeValueMap attrs;

  // Spin thru each update
  for (int i=0; i < req->updates_size(); i++) {

    ::util::StatusOr<::stratum::hal::phal::Path> attr_res;

    // Get the update
    const auto& update = req->updates(i);

    // Convert to PhalDB Path
    switch (update.query_case()) {
      case ::stratum::hal::phal::Update::kStr: {
        attr_res = ParseQuery(update.str());
        break;
      }
      case ::stratum::hal::phal::Update::kPath: {
        attr_res = ToPhalDBPath(update.path());
        break;
      }
      default:
        attr_res = MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid update query";
    }

    if (!attr_res.ok()) {
        LOG(ERROR) << "Set update %d failed " << i
                   << ": " << attr_res.status().error_message();
        // If we got an error set the top level status
        status = attr_res.status();
        results.push_back(attr_res.status());
        continue;
    }

    auto path = attr_res.ConsumeValueOrDie();
    
    // Create attribute path:val pair base on value type
    switch (update.value().value_case()) {
      case ::stratum::hal::phal::UpdateValue::kDoubleVal: {
        attrs[path] = update.value().double_val();
        break;
      }
      case ::stratum::hal::phal::UpdateValue::kFloatVal: {
        attrs[path] = update.value().float_val();
        break;
      }
      case ::stratum::hal::phal::UpdateValue::kInt32Val: {
        attrs[path] = update.value().int32_val();
        break;
      }
      case ::stratum::hal::phal::UpdateValue::kInt64Val: {
        attrs[path] = update.value().int64_val();
        break;
      }
      case ::stratum::hal::phal::UpdateValue::kUint32Val: {
        attrs[path] = update.value().uint32_val();
        break;
      }
      case ::stratum::hal::phal::UpdateValue::kUint64Val: {
        attrs[path] = update.value().uint64_val();
        break;
      }
      case ::stratum::hal::phal::UpdateValue::kBoolVal: {
        attrs[path] = update.value().bool_val();
        break;
      }
      case ::stratum::hal::phal::UpdateValue::kStringVal: {
        attrs[path] = update.value().string_val();
        break;
      }
      case ::stratum::hal::phal::UpdateValue::kBytesVal: {
        attrs[path] = update.value().bytes_val();
        break;
      }
      default: {
        attr_res = MAKE_ERROR(ERR_INVALID_PARAM) << "Unknown value type";
        break;
      }
    }

    // Push status onto results stack
    results.push_back(attr_res.status());

    // If we got an error set the top level status
    if (!attr_res.ok()) {
        status = attr_res.status();
    }
  }

  // Do Set if we have no errors
  if (status.ok()) {
    // Note: all updates are passed down to PhalDB as one Set call
    //       so we won't get individual status on each adapter attribute 
    //       update.
    results = {};

    // Do set for all attribute pairs
    auto adapter = 
        absl::make_unique<::stratum::hal::phal::Adapter>(phal_interface_);
    status = adapter->Set(attrs);
  }

  return ToGrpcStatus(status, results);
}

::grpc::Status PhalDBService::Subscribe(::grpc::ServerContext* context,
    const ::stratum::hal::phal::SubscribeRequest* req,
    ::grpc::ServerWriter<::stratum::hal::phal::SubscribeResponse>* stream) {

  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, PhalDBService, 
                           Subsribe, context);

  ::util::Status status;
  ::util::StatusOr<::stratum::hal::phal::Path> result;

  // Convert to PhalDB Path
  switch (req->query_case()) {
    case ::stratum::hal::phal::SubscribeRequest::kStr: {
        result = ParseQuery(req->str());
        break;
    }
    case ::stratum::hal::phal::SubscribeRequest::kPath: {
        result = ToPhalDBPath(req->path());
        break;
    }
    default:
        return ::grpc::Status(
            ::grpc::StatusCode::INVALID_ARGUMENT,
            "Invalid query in Subscribe request.");
  }

  // Save status
  status = result.status();

  // If conversion worked
  if (result.ok()) {
    auto path = result.ConsumeValueOrDie();

    // Create writer and reader channels
    std::shared_ptr<Channel<::stratum::hal::phal::PhalDB>> channel =
        Channel<::stratum::hal::phal::PhalDB>::Create(128);

    {
        // Lock subscriber channels
        absl::MutexLock l(&subscriber_thread_lock_);

        // Save channel to subscriber channel map
        subscriber_channels_[pthread_self()] = channel;
    }

    auto writer = 
            ChannelWriter<::stratum::hal::phal::PhalDB>::Create(channel);

    auto reader = 
        ChannelReader<::stratum::hal::phal::PhalDB>::Create(channel);

    // Issue the subscribe
    auto adapter = 
        absl::make_unique<::stratum::hal::phal::Adapter>(phal_interface_);
    status = adapter->Subscribe({path}, std::move(writer), 
                                 absl::Seconds(req->polling_interval()));

    // If Subscribe ok
    if (status.ok()) {

        // Loop around processing messages from the PhalDB writer
        // Note: if the client dies we'll only close the channel
        //       and thus cancel the PhalDB subscription once we
        //       get something from the PhalDB subscription (i.e.
        //       if the poll timer expires and something has changed).
        //       We could potentially put something in here to check
        //       the stream and channel for changes but for now this
        //       will do.
        do {
            ::stratum::hal::phal::PhalDB phaldb_resp;
            int code = reader->Read(&phaldb_resp, 
                                    absl::InfiniteDuration()).error_code();
    
            // Exit if the channel is closed
            if (code == ERR_CANCELLED) {
                LOG(ERROR) << "PhalDB Subscribe closed the channel";
                status = MAKE_ERROR(ERR_INTERNAL) 
                            << "PhalDB Subscribe closed the channel";
                break;
            }
    
            // Error if read timesout
            if (code == ERR_ENTRY_NOT_FOUND) {
                LOG(ERROR) << "Subscribe read with infinite timeout "
                           << "failed with ENTRY_NOT_FOUND.";
                status = MAKE_ERROR(ERR_INTERNAL) 
                            << "Subscribe read with infinite timeout "
                            << "failed with ENTRY_NOT_FOUND.";
                continue;
            }

            // Send message to client
            ::stratum::hal::phal::SubscribeResponse resp;
            *resp.mutable_phal_db() = phaldb_resp;
    
            // If Write fails then break out of the loop
            if (!stream->Write(resp)) {
                LOG(ERROR) << "Subscribe stream write failed";
                status = MAKE_ERROR(ERR_INTERNAL) 
                            << "Subscribe stream write failed";
                break;
            }
        } while (true);
    }

    {
        // Lock subscriber channels
        absl::MutexLock l(&subscriber_thread_lock_);

        // Close the channel which will then cause the PhalDB writer
        // to close and exit
        channel->Close();
        subscriber_channels_.erase(pthread_self());
    }
  }

  // Convert to grpc status and return
  return ToGrpcStatus(status, {});
}

}  // namespace hal
}  // namespace stratum
