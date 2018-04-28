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


#ifndef STRATUM_PUBLIC_LIB_ERROR_H_
#define STRATUM_PUBLIC_LIB_ERROR_H_

#include "third_party/stratum/public/proto/error.pb.h"
#include "util/task/error_space.h"

namespace stratum {
namespace error {
class StratumErrorSpace : public ::util::ErrorSpaceImpl<StratumErrorSpace> {
 public:
  static ::std::string space_name();
  static ::std::string code_to_string(int code);
  static ::util::error::Code canonical_code(const ::util::Status& status);
};
}  // namespace error

// StratumErrorSpace returns the singleton instance to be used through
// out the code.
const ::util::ErrorSpace* StratumErrorSpace();

}  // namespace stratum

#endif  // STRATUM_PUBLIC_LIB_ERROR_H_
