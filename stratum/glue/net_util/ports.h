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


#ifndef STRATUM_GLUE_NET_UTIL_PORTS_H_
#define STRATUM_GLUE_NET_UTIL_PORTS_H_

namespace stratum {

#define PickUnusedPortOrDie PickUnusedIpv4PortOrDie

// The same as PickUnusedPortOrDie() in google3/net/util/ports.h. We have to
// have a copy here due to dependency to an unsupported PPC compiler. We also
// changed the name to get rid of ipv6-hostile warning. In future we expect to
// get rid of this function.
int PickUnusedIpv4PortOrDie();

}  // namespace stratum

#endif  // STRATUM_GLUE_NET_UTIL_PORTS_H_
