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


#include "stratum/hal/lib/p4/p4_runtime_interface.h"

namespace stratum {
namespace hal {

// This file's only purpose is to provide storage for the P4 runtime
// API pointer.
P4RuntimeInterface* P4RuntimeInterface::instance_ = nullptr;

}  // namespace hal
}  // namespace stratum
