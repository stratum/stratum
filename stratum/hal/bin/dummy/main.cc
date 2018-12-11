/* Copyright 2018-present Open Networking Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/common/hal.h"
#include "stratum/hal/lib/dummy/dummy_switch.h"
#include "stratum/hal/lib/dummy/dummy_chassis_mgr.h"
#include "stratum/hal/lib/dummy/dummy_phal.h"
#include "stratum/hal/lib/dummy/dummy_box.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

// Entry point of Dummy Switch
int DummySwitchMain(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);

  auto dummy_box = DummyBox::GetSingleton();
  dummy_box->Start();

  PhalInterface* dummy_phal = DummyPhal::CreateSingleton();
  DummyChassisManager* chassis_mgr = DummyChassisManager::GetSingleton();

  std::unique_ptr<DummySwitch> dummy_switch =
    DummySwitch::CreateInstance(dummy_phal, chassis_mgr);

  auto auth_policy_checker = AuthPolicyChecker::CreateInstance();
  auto credentials_manager = CredentialsManager::CreateInstance();
  auto* hal = Hal::CreateSingleton(stratum::hal::OPERATION_MODE_SIM,
                                   dummy_switch.get(),
                                   auth_policy_checker.get(),
                                   credentials_manager.get());

  if (!hal) {
    LOG(ERROR) << "Failed to create the Hal instance.";
    return -1;
  }

  ::util::Status status = hal->Setup();
  if (!status.ok()) {
    LOG(ERROR)
        << "Error when setting up HAL (but we will continue running): "
        << status.error_message();
  }
  status = hal->Run();  // blocking
  if (!status.ok()) {
    LOG(ERROR) << "Error when running the HAL: " << status.error_message();
    return -1;
  }

  dummy_box->Shutdown();

  LOG(INFO) << "See you later!";
  return 0;
}

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum

int main(int argc, char* argv[]) {
  return stratum::hal::dummy_switch::DummySwitchMain(argc, argv);
}
