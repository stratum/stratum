/*
 * Copyright 2018 Open Networking Foundation
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

#ifndef STRATUM_GLUE_GTL_SOURCE_LOCATION_H_
#define STRATUM_GLUE_GTL_SOURCE_LOCATION_H_

#include <string>

namespace stratum {
namespace gtl {

class source_location {
 public:
    source_location(const std::string file, const int line) : file_(file),
      line_(line) {}
    std::string file_name() const { return file_; }
    int line() const { return line_; }
 private:
    const std::string file_;
    const int         line_;
};

}  // namespace gtl
}  // namespace stratum

#define GTL_LOC stratum::gtl::source_location(__FILE__, __LINE__)

#endif  // STRATUM_GLUE_GTL_SOURCE_LOCATION_H_
