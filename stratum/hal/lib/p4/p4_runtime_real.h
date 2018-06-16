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


// This file defines a P4RuntimeInterface subclass that uses the real
// P4 runtime library APIs.

#ifndef STRATUM_HAL_LIB_P4_P4_RUNTIME_REAL_H_
#define STRATUM_HAL_LIB_P4_P4_RUNTIME_REAL_H_

#include "stratum/hal/lib/p4/p4_runtime_interface.h"

namespace stratum {
namespace hal {

class P4RuntimeReal : P4RuntimeInterface {
 public:
  virtual ~P4RuntimeReal() {}

  p4::config::v1::P4Ids::Prefix GetResourceTypeFromID(
      pi::proto::util::p4_id_t object_id) override;

  // Factory method for getting the P4RuntimeReal instance; creates a new
  // instance if no other P4RuntimeInterface exists.
  static P4RuntimeInterface* GetSingleton();

  // P4RuntimeReal is neither copyable nor movable.
  P4RuntimeReal(const P4RuntimeReal&) = delete;
  P4RuntimeReal& operator=(const P4RuntimeReal&) = delete;

 private:
  // Constructor is private; use GetSingleton.
  P4RuntimeReal() {}
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_RUNTIME_REAL_H_
