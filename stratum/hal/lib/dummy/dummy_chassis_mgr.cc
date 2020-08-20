// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/dummy/dummy_chassis_mgr.h"

#include <memory>
#include <string>

#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/common/common.pb.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

using Request = stratum::hal::DataRequest::Request;

DummyChassisManager* chassis_mgr_singleton_ = nullptr;

DummyChassisManager::DummyChassisManager()
: dummy_box_(DummyBox::GetSingleton()) { }

DummyChassisManager::~DummyChassisManager() {}

::util::Status
DummyChassisManager::PushChassisConfig(const ChassisConfig& config) {
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::Status
DummyChassisManager::VerifyChassisConfig(const ChassisConfig& config) {
  LOG(INFO) << __FUNCTION__;
  // TODO(Yi Tseng) Verify the chassis config.
  return ::util::OkStatus();
}

::util::Status DummyChassisManager::Shutdown() {
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::Status DummyChassisManager::Freeze() {
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

::util::Status DummyChassisManager::Unfreeze() {
  LOG(INFO) << __FUNCTION__;
  return ::util::OkStatus();
}

DummyChassisManager* DummyChassisManager::GetSingleton() {
  if (chassis_mgr_singleton_ == nullptr) {
    chassis_mgr_singleton_ = new DummyChassisManager();
  }
  return chassis_mgr_singleton_;
}

::util::Status DummyChassisManager::RegisterEventNotifyWriter(
      std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) {
  if (chassis_event_writer_) {
    return MAKE_ERROR(ERR_INTERNAL) << "Event notify writer already exists";
  }
  chassis_event_writer_ = writer;
  dummy_box_->RegisterChassisEventNotifyWriter(chassis_event_writer_);
  return ::util::OkStatus();
}

::util::Status DummyChassisManager::UnregisterEventNotifyWriter() {
  dummy_box_->UnregisterChassisEventNotifyWriter();
  return ::util::OkStatus();
}

::util::StatusOr<DataResponse>
DummyChassisManager::RetrieveChassisData(const Request request) {
  // TODO(Yi Tseng): Implement this method.
  switch (request.request_case()) {
    case Request::kNodeInfo: {
      DataResponse resp;
      NodeInfo* node_info = resp.mutable_node_info();
      node_info->set_vendor_name("dummy vendor");
      node_info->set_chip_name("dummy chip name");
      return resp;
    }
    default:
      return MAKE_ERROR(ERR_INTERNAL) << "Not supported yet!";
  }
}

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum

