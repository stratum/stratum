// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/tai/taish_client.h"

#include <algorithm>
#include <string>
#include <utility>

#include "gflags/gflags.h"
#include "grpcpp/grpcpp.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/lib/macros.h"

DEFINE_string(taish_addr, "localhost:50051", "The gRPC address of TAI shell.");

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

TaishClient* TaishClient::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex TaishClient::init_lock_(absl::kConstInit);

util::Status TaishClient::Initialize() {
  CHECK_RETURN_IF_FALSE(!initialized_);
  CHECK_RETURN_IF_FALSE(!FLAGS_taish_addr.empty());
  auto channel = ::grpc::CreateChannel(FLAGS_taish_addr,
                                       grpc::InsecureChannelCredentials());
  taish_stub_ = ::taish::TAI::NewStub(channel);

  // Gets object id of all module, network interfaces, and host interfaces.
  {
    grpc::ClientContext context;
    taish::ListModuleRequest list_module_req;
    taish::ListModuleResponse list_module_resp;

    auto modules_reader = taish_stub_->ListModule(&context, list_module_req);
    while (modules_reader->Read(&list_module_resp)) {
      const auto& module = list_module_resp.module();
      modules_.push_back(module);
    }
    modules_reader->Finish();
  }

  // Get attributes from module, network interace, and host interface.

  // Module attributes
  {
    grpc::ClientContext ctx;
    taish::ListAttributeMetadataRequest req;
    taish::ListAttributeMetadataResponse resp;
    req.set_object_type(taish::MODULE);
    auto attr_meta_reader = taish_stub_->ListAttributeMetadata(&ctx, req);
    while (attr_meta_reader->Read(&resp)) {
      const auto& attr_meta = resp.metadata();
      module_attr_map_[attr_meta.name()] = attr_meta.attr_id();
    }
    attr_meta_reader->Finish();
  }

  // Network interface attributes
  {
    grpc::ClientContext ctx;
    taish::ListAttributeMetadataRequest req;
    taish::ListAttributeMetadataResponse resp;
    req.set_object_type(taish::NETIF);
    auto attr_meta_reader = taish_stub_->ListAttributeMetadata(&ctx, req);
    while (attr_meta_reader->Read(&resp)) {
      const auto& attr_meta = resp.metadata();
      netif_attr_map_[attr_meta.name()] = attr_meta.attr_id();
    }
    attr_meta_reader->Finish();
  }

  // Host interface attributes
  {
    grpc::ClientContext ctx;
    taish::ListAttributeMetadataRequest req;
    taish::ListAttributeMetadataResponse resp;
    req.set_object_type(taish::HOSTIF);
    auto attr_meta_reader = taish_stub_->ListAttributeMetadata(&ctx, req);
    while (attr_meta_reader->Read(&resp)) {
      const auto& attr_meta = resp.metadata();
      hostif_attr_map_[attr_meta.name()] = attr_meta.attr_id();
    }
    attr_meta_reader->Finish();
  }

  initialized_ = true;
  return util::OkStatus();
}

util::StatusOr<std::vector<uint64>> TaishClient::GetModuleIds() {
  absl::ReaderMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  std::vector<uint64> oids;
  for (const auto& module : modules_) {
    oids.push_back(module.oid());
  }
  return oids;
}

util::StatusOr<std::vector<uint64>> TaishClient::GetNetworkInterfaceIds(
    const uint64 module_id) {
  absl::ReaderMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  auto it = std::find_if(
      modules_.begin(), modules_.end(),
      [module_id](taish::Module m) { return m.oid() == module_id; });
  CHECK_RETURN_IF_FALSE(it != modules_.end())
      << "Cannot find module with id " << module_id;
  taish::Module module = *it;
  std::vector<uint64> oids;
  for (const auto& netif : module.netifs()) {
    oids.push_back(netif.oid());
  }
  return oids;
}

util::StatusOr<std::vector<uint64>> TaishClient::GetHostInterfaceIds(
    const uint64 module_id) {
  absl::ReaderMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  auto it = std::find_if(
      modules_.begin(), modules_.end(),
      [module_id](taish::Module m) { return m.oid() == module_id; });
  CHECK_RETURN_IF_FALSE(it != modules_.end())
      << "Can not find module with id " << module_id;
  taish::Module module = *it;
  std::vector<uint64> oids;
  for (const auto& hostif : module.hostifs()) {
    oids.push_back(hostif.oid());
  }
  return oids;
}

util::StatusOr<uint64> TaishClient::GetTxLaserFrequency(const uint64 netif_id) {
  absl::ReaderMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  ASSIGN_OR_RETURN(
      auto attr_str_val,
      GetAttribute(netif_id, netif_attr_map_[kNetIfAttrTxLaserFreq]));
  uint64 result;
  CHECK_RETURN_IF_FALSE(absl::SimpleAtoi<uint64>(attr_str_val, &result));
  return result;
}

util::StatusOr<double> TaishClient::GetCurrentInputPower(
    const uint64 netif_id) {
  absl::ReaderMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  ASSIGN_OR_RETURN(
      auto attr_str_val,
      GetAttribute(netif_id, netif_attr_map_[kNetIfAttrCurrentInputPower]));
  double result;
  CHECK_RETURN_IF_FALSE(absl::SimpleAtod(attr_str_val, &result));
  return result;
}

util::StatusOr<double> TaishClient::GetCurrentOutputPower(
    const uint64 netif_id) {
  absl::ReaderMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  ASSIGN_OR_RETURN(
      auto attr_str_val,
      GetAttribute(netif_id, netif_attr_map_[kNetIfAttrCurrentOutputPower]));
  double result;
  CHECK_RETURN_IF_FALSE(absl::SimpleAtod(attr_str_val, &result));
  return result;
}

util::StatusOr<double> TaishClient::GetTargetOutputPower(
    const uint64 netif_id) {
  absl::ReaderMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  ASSIGN_OR_RETURN(
      auto attr_str_val,
      GetAttribute(netif_id, netif_attr_map_[kNetIfAttrOutputPower]));
  double result;
  CHECK_RETURN_IF_FALSE(absl::SimpleAtod(attr_str_val, &result));
  return result;
}

util::StatusOr<uint64> TaishClient::GetModulationFormat(const uint64 netif_id) {
  absl::ReaderMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  ASSIGN_OR_RETURN(
      auto attr_str_val,
      GetAttribute(netif_id, netif_attr_map_[kNetIfAttrModulationFormat]));
  return GetModulationFormatIds(attr_str_val);
}

util::Status TaishClient::SetTargetOutputPower(const uint64 netif_id,
                                               const double power) {
  absl::ReaderMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  return SetAttribute(netif_id, netif_attr_map_[kNetIfAttrOutputPower],
                      std::to_string(power));
}

util::Status TaishClient::SetModulationFormat(const uint64 netif_id,
                                              const uint64 mod_format) {
  absl::ReaderMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  ASSIGN_OR_RETURN(auto modulation_format_name,
                   GetModulationFormatName(mod_format));
  return SetAttribute(netif_id, netif_attr_map_[kNetIfAttrModulationFormat],
                      modulation_format_name);
}

util::Status TaishClient::SetTxLaserFrequency(const uint64 netif_id,
                                              const uint64 frequency) {
  absl::ReaderMutexLock l(&init_lock_);
  CHECK_RETURN_IF_FALSE(initialized_);
  return SetAttribute(netif_id, netif_attr_map_[kNetIfAttrTxLaserFreq],
                      std::to_string(frequency));
}

util::Status TaishClient::Shutdown() {
  absl::WriterMutexLock l(&init_lock_);
  initialized_ = false;
  return util::OkStatus();
}

util::StatusOr<std::string> TaishClient::GetAttribute(uint64 obj_id,
                                                      uint64 attr_id) {
  grpc::ClientContext context;
  taish::GetAttributeRequest request;
  taish::GetAttributeResponse response;

  request.set_oid(obj_id);
  request.mutable_serialize_option()->set_value_only(true);
  request.mutable_serialize_option()->set_human(false);
  request.mutable_serialize_option()->set_json(false);
  request.mutable_attribute()->set_attr_id(attr_id);

  auto status = taish_stub_->GetAttribute(&context, request, &response);
  CHECK_RETURN_IF_FALSE(status.ok()) << status.error_message();
  return response.attribute().value();
}

util::Status TaishClient::SetAttribute(uint64 obj_id, uint64 attr_id,
                                       std::string value) {
  grpc::ClientContext context;
  taish::SetAttributeRequest request;
  taish::SetAttributeResponse response;

  request.set_oid(obj_id);
  request.mutable_serialize_option()->set_value_only(true);
  request.mutable_serialize_option()->set_human(false);
  request.mutable_serialize_option()->set_json(false);
  request.mutable_attribute()->set_attr_id(attr_id);
  request.mutable_attribute()->set_value(value);

  auto status = taish_stub_->SetAttribute(&context, request, &response);
  CHECK_RETURN_IF_FALSE(status.ok()) << status.error_message();
  return util::OkStatus();
}

util::StatusOr<uint64> TaishClient::GetModulationFormatIds(
    const std::string& modulation_format) {
  CHECK_RETURN_IF_FALSE(kModulationFormatIds.contains(modulation_format))
      << "Unknown modulation format " << modulation_format;
  return kModulationFormatIds.at(modulation_format);
}

util::StatusOr<std::string> TaishClient::GetModulationFormatName(
    const uint64 id) {
  const auto it =
      std::find_if(kModulationFormatIds.begin(), kModulationFormatIds.end(),
                   [id](const std::pair<std::string, uint64>& kv) {
                     return kv.second == id;
                   });
  CHECK_RETURN_IF_FALSE(it != kModulationFormatIds.end())
      << "Invalid modulation format id " << id;
  return it->first;
}

TaishClient* TaishClient::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new TaishClient();
    if (!singleton_->Initialize().ok()) {
      LOG(ERROR) << "Failed to initialize TaishClient";
      delete singleton_;
      return nullptr;
    }
  }
  return singleton_;
}

TaishClient::TaishClient() {}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
