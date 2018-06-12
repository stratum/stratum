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

//FIXME this is a temporary hack

#ifndef STRATUM_GLUE_GTL_SOURCE_LOCATION_H_
#define STRATUM_GLUE_GTL_SOURCE_LOCATION_H_

namespace stratum {
namespace gtl {

class source_location {
public:
    std::string file_name() { return "test"; }
    int line() { return 1; }

};

}  // namespace gtl
}  // namespace stratum

#define GTL_LOC stratum::gtl::source_location()

#endif  // STRATUM_GLUE_GTL_SOURCE_LOCATION_H_