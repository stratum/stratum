/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_COMMON_WRITER_INTERFACE_H_
#define STRATUM_HAL_LIB_COMMON_WRITER_INTERFACE_H_

namespace stratum {
namespace hal {

// An interface for a wrapper around the Write operation for various data
// transport mechanisms, e.g. Stratum Channel, gRPC ServerWriter. This class
// makes the SwitchInterface class more abstract and eliminates the dependency
// to gRPC as well as Stratum-internal constructs.
template <typename T>
class WriterInterface {
 public:
  virtual ~WriterInterface() {}
  // Blocking Write() operation which passes a message of type T into the
  // underlying transfer mechanism.
  virtual bool Write(const T& msg) = 0;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  WriterInterface() {}
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_WRITER_INTERFACE_H_
