// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/common/hal.h"
#include "stratum/hal/lib/dummy/dummy_box.h"
#include "stratum/hal/lib/dummy/dummy_chassis_mgr.h"
#include "stratum/hal/lib/dummy/dummy_switch.h"
#include "stratum/hal/lib/phal/phal.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

// Entry point of Dummy Switch
::util::Status Main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();

  auto dummy_box = DummyBox::GetSingleton();
  dummy_box->Start();

  PhalInterface* phal = phal::Phal::CreateSingleton();
  DummyChassisManager* chassis_mgr = DummyChassisManager::GetSingleton();

  std::unique_ptr<DummySwitch> dummy_switch =
      DummySwitch::CreateInstance(phal, chassis_mgr);

  auto auth_policy_checker = AuthPolicyChecker::CreateInstance();
  ASSIGN_OR_RETURN(auto credentials_manager,
                   CredentialsManager::CreateInstance());
  auto* hal = Hal::CreateSingleton(
      stratum::hal::OPERATION_MODE_SIM, dummy_switch.get(),
      auth_policy_checker.get(), credentials_manager.get());
  RET_CHECK(hal) << "Failed to create the Hal instance.";
  ::util::Status status = hal->Setup();
  if (!status.ok()) {
    LOG(ERROR) << "Error when setting up HAL (but we will continue running): "
               << status.error_message();
  }
  RETURN_IF_ERROR(hal->Run());  // blocking
  dummy_box->Shutdown();

  LOG(INFO) << "See you later!";
  return ::util::OkStatus();
}

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum

int main(int argc, char* argv[]) {
  return stratum::hal::dummy_switch::Main(argc, argv).error_code();
}
