// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

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
