/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


// This file defines an interface to P4 runtime library APIs.

#ifndef STRATUM_HAL_LIB_P4_P4_RUNTIME_INTERFACE_H_
#define STRATUM_HAL_LIB_P4_P4_RUNTIME_INTERFACE_H_

#include "PI/proto/util.h"

namespace stratum {
namespace hal {

class P4RuntimeInterface {
 public:
  virtual ~P4RuntimeInterface() {}

  virtual pi::proto::util::P4ResourceType GetResourceTypeFromID(
      pi::proto::util::p4_id_t object_id) = 0;

  // This static accessor fetches the singleton instance.
  static P4RuntimeInterface* instance() { return instance_; }

 protected:
  static void set_instance(P4RuntimeInterface* instance) {
    instance_ = instance;
  }

 private:
  static P4RuntimeInterface* instance_;

  // Friendly tests can specify their own mock instance.
  friend class PrintP4ObjectIDTest;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_RUNTIME_INTERFACE_H_
