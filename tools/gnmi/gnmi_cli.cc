// Copyright 2019-present Open Networking Foundation
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

#include <csignal>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <regex>  // NOLINT

#define STRIP_FLAG_HELP 1  // remove additional flag help text from gflag
#include "gflags/gflags.h"
#include "gnmi/gnmi.grpc.pb.h"

const char kUsage[] =
R"USAGE(usage: gnmi-cli.py [-h] [-grpc_addr GRPC_ADDR] [-bool_val BOOL_VAL]
                   [-int_val INT_VAL] [-uint_val UINT_VAL]
                   [-string_val STRING_VAL] [-float_val FLOAT_VAL]
                   {get,set,sub} path

Basic gNMI CLI

positional arguments:
  {get,set,sub,cap}         gNMI command
  path                      gNMI path

optional arguments:
  --help            show this help message and exit
  --grpc_addr GRPC_ADDR    gNMI server address
  --bool_val BOOL_VAL      [SetRequest only] Set boolean value
  --int_val INT_VAL        [SetRequest only] Set int value (64-bit)
  --uint_val UINT_VAL      [SetRequest only] Set uint value (64-bit)
  --string_val STRING_VAL  [SetRequest only] Set string value
  --float_val FLOAT_VAL    [SetRequest only] Set float value
  --interval INTERVAL      [Sample subscribe only] Sample subscribe poll interval in ms
  --replace                [SetRequest only] Use replace instead of update
)USAGE";

#define PRINT_MSG(msg, prompt) \
  std::cout << prompt << std::endl; \
  std::cout << msg.DebugString() << std::endl;

#define LOG_IF_NOT_OK(status) \
  if (!status.ok()) { \
    std::cout << status.error_message() << std::endl; \
  }

#define CHECK_AND_PRINT_RESP(status, msg) \
  if (status.ok()) { \
    PRINT_MSG(msg, "RESPONSE") \
  }

DEFINE_string(grpc_addr, "127.0.0.1:28000", "gNMI server address");
DEFINE_string(bool_val, "", "Boolean value to be set");
DEFINE_string(int_val, "", "Integer value to be set (64-bit)");
DEFINE_string(uint_val, "", "Unsigned integer value to be set (64-bit)");
DEFINE_string(string_val, "", "String value to be set");
DEFINE_string(float_val, "", "Floating point value to be set");

DEFINE_uint64(interval, 5000, "Subscribe poll interval in ms");
DEFINE_bool(replace, false, "Use replace instead of update");

namespace stratum {
namespace tools {
namespace gnmi {

bool str_to_bool(std::string str) {
  return (str == "y") || (str == "true") || (str == "t")
         || (str == "yes") || (str == "1");
}

void add_path_elem(std::string elem_name, std::string elem_kv,
                   ::gnmi::PathElem* elem) {
  elem->set_name(elem_name);
  if (!elem_kv.empty()) {
    std::regex ex("\\[([^=]+)=([^\\]]+)\\]");
    std::smatch sm;
    std::regex_match(elem_kv, sm, ex);
    (*elem->mutable_key())[sm.str(1)] = sm.str(2);
  }
}

void build_gnmi_path(std::string path_str, ::gnmi::Path* path) {
  std::regex ex("/([^/\\[]+)(\\[([^=]+=[^\\]]+)\\])?");
  std::sregex_iterator iter(path_str.begin(), path_str.end(), ex);
  std::sregex_iterator end;
  while (iter != end) {
    std::smatch sm = *iter;
    auto* elem = path->add_elem();
    add_path_elem(sm.str(1), sm.str(2), elem);
    iter++;
  }
}

::gnmi::GetRequest build_gnmi_get_req(std::string path) {
  ::gnmi::GetRequest req;
  build_gnmi_path(path, req.add_path());
  req.set_encoding(::gnmi::PROTO);
  return req;
}

::gnmi::SetRequest build_gnmi_set_req(std::string path) {
  ::gnmi::SetRequest req;
  ::gnmi::Update* update;
  if (FLAGS_replace) {
    update = req.add_replace();
  } else {
    update = req.add_update();
  }
  build_gnmi_path(path, update->mutable_path());
  if (!FLAGS_bool_val.empty()) {
    update->mutable_val()->set_bool_val(str_to_bool(FLAGS_bool_val));
  } else if (!FLAGS_int_val.empty()) {
    update->mutable_val()->set_int_val(stoll(FLAGS_int_val));
  } else if (!FLAGS_uint_val.empty()) {
    update->mutable_val()->set_uint_val(stoull(FLAGS_uint_val));
  } else if (!FLAGS_float_val.empty()) {
    update->mutable_val()->set_float_val(stof(FLAGS_float_val));
  }  else if (!FLAGS_string_val.empty()) {
    update->mutable_val()->set_string_val(FLAGS_string_val);
  } else {
    std::cout << "No typed value set" << std::endl;
  }
  return req;
}

::gnmi::SetRequest build_gnmi_del_req(std::string path) {
  ::gnmi::SetRequest req;
  auto* del = req.add_delete_();
  build_gnmi_path(path, del);
  return req;
}

::gnmi::SubscribeRequest build_gnmi_sub_onchange_req(std::string path) {
  ::gnmi::SubscribeRequest sub_req;
  auto* sub_list = sub_req.mutable_subscribe();
  sub_list->set_mode(::gnmi::SubscriptionList::STREAM);
  sub_list->set_updates_only(true);
  auto* sub = sub_list->add_subscription();
  sub->set_mode(::gnmi::ON_CHANGE);
  build_gnmi_path(path, sub->mutable_path());
  return sub_req;
}

::gnmi::SubscribeRequest
build_gnmi_sub_sample_req(std::string path,
                          ::google::protobuf::uint64 interval) {
  ::gnmi::SubscribeRequest sub_req;
  auto* sub_list = sub_req.mutable_subscribe();
  sub_list->set_mode(::gnmi::SubscriptionList::STREAM);
  sub_list->set_updates_only(true);
  auto* sub = sub_list->add_subscription();
  sub->set_mode(::gnmi::SAMPLE);
  sub->set_sample_interval(interval);
  build_gnmi_path(path, sub->mutable_path());
  return sub_req;
}

::grpc::ClientReaderWriterInterface
    <::gnmi::SubscribeRequest, ::gnmi::SubscribeResponse>* stream_reader_writer;

int Main(int argc, char** argv) {
  if (argc < 3) {
    std::cout << kUsage << std::endl;
    return -1;
  }
  ::grpc::ClientContext ctx;
  std::string cmd = std::string(argv[1]);
  std::string path = std::string(argv[2]);
  auto channel = ::grpc::CreateChannel(FLAGS_grpc_addr,
      ::grpc::InsecureChannelCredentials());
  auto stub = ::gnmi::gNMI::NewStub(channel);
  ::grpc::Status status;

  if (cmd == "get") {
    ::gnmi::GetRequest req = build_gnmi_get_req(path);
    PRINT_MSG(req, "REQUEST")
    ::gnmi::GetResponse resp;
    status = stub->Get(&ctx, req, &resp);
    CHECK_AND_PRINT_RESP(status, resp)
  } else if (cmd == "set") {
    ::gnmi::SetRequest req = build_gnmi_set_req(path);
    PRINT_MSG(req, "REQUEST")
    ::gnmi::SetResponse resp;
    status = stub->Set(&ctx, req, &resp);
    CHECK_AND_PRINT_RESP(status, resp)
  } else if (cmd == "del") {
    ::gnmi::SetRequest req = build_gnmi_del_req(path);
    PRINT_MSG(req, "REQUEST")
    ::gnmi::SetResponse resp;
    status = stub->Set(&ctx, req, &resp);
    CHECK_AND_PRINT_RESP(status, resp)
  } else if (cmd == "sub-onchange") {
    stream_reader_writer = stub->Subscribe(&ctx).get();
    ::gnmi::SubscribeRequest req = build_gnmi_sub_onchange_req(path);
    PRINT_MSG(req, "REQUEST")
    if (!stream_reader_writer->Write(req)) {
      std::cout << "Can not write request" << std::endl;
    }
    ::gnmi::SubscribeResponse resp;
    while (stream_reader_writer->Read(&resp)) {
      PRINT_MSG(resp, "RESPONSE")
    }
    status = stream_reader_writer->Finish();
    LOG_IF_NOT_OK(status);
  } else if (cmd == "sub-sample") {
    stream_reader_writer = stub->Subscribe(&ctx).get();
    ::gnmi::SubscribeRequest req =
        build_gnmi_sub_sample_req(path, FLAGS_interval);
    PRINT_MSG(req, "REQUEST")
    if (!stream_reader_writer->Write(req)) {
      std::cout << "Can not write request" << std::endl;
    }
    ::gnmi::SubscribeResponse resp;
    while (stream_reader_writer->Read(&resp)) {
      PRINT_MSG(resp, "RESPONSE")
    }
    status = stream_reader_writer->Finish();
    LOG_IF_NOT_OK(status);
  } else if (cmd == "cap") {
    ::gnmi::CapabilityRequest req;
    PRINT_MSG(req, "REQUEST")
    ::gnmi::CapabilityResponse resp;
    stub->Capabilities(&ctx, req, &resp);
    CHECK_AND_PRINT_RESP(status, resp)
  } else {
    std::cout << "Unknown command: " << cmd << std::endl;
  }
  return 0;
}

void HandleSignal(int signal) {
  // Send empty subscribe request to close the stream.
  ::gnmi::SubscribeRequest req;
  if (!stream_reader_writer->Write(req)) {
    std::cout << "Can not write request" << std::endl;
  }
}

}  // namespace gnmi
}  // namespace tools
}  // namespace stratum

int main(int argc, char** argv) {
  ::gflags::SetUsageMessage(kUsage);
  ::gflags::ParseCommandLineFlags(&argc, &argv, true);
  std::signal(SIGINT, stratum::tools::gnmi::HandleSignal);
  return stratum::tools::gnmi::Main(argc, argv);
}
