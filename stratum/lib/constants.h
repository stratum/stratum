// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_CONSTANTS_H_
#define STRATUM_LIB_CONSTANTS_H_

#include "stratum/glue/integral_types.h"

namespace stratum {

// Port speed constants.
constexpr uint64 kBitsPerGigabit = 1000000000ULL;
constexpr uint64 kBitsPerMegabit = 1000000ULL;
constexpr uint64 kOneGigBps = 1 * kBitsPerGigabit;
constexpr uint64 kTenGigBps = 10 * kBitsPerGigabit;
constexpr uint64 kTwentyGigBps = 20 * kBitsPerGigabit;
constexpr uint64 kTwentyFiveGigBps = 25 * kBitsPerGigabit;
constexpr uint64 kFortyGigBps = 40 * kBitsPerGigabit;
constexpr uint64 kFiftyGigBps = 50 * kBitsPerGigabit;
constexpr uint64 kHundredGigBps = 100 * kBitsPerGigabit;
constexpr uint64 kTwoHundredGigBps = 200 * kBitsPerGigabit;
constexpr uint64 kFourHundredGigBps = 400 * kBitsPerGigabit;

// Default paths for membership info & authorization policy proto files.
constexpr char kDefaultMembershipInfoFilePath[] =
    "/mnt/region_config/switchstack/keys/membership_info.proto.txt";
constexpr char kDefaultAuthPolicyFilePath[] =
    "/mnt/region_config/switchstack/keys/authorization_policy.proto.txt";

// This URL is used by a local Stratum stub binary running on the switch to
// talk to Stratum process over an insecure connection.
constexpr char kLocalStratumUrl[] = "localhost:28000";

// This URL is used by external gNMI, gNOI and P4Runtime clients.
// TCP port 9339 is an IANA-reserved port for gNMI and gNOI.
// TCP port 9559 is an IANA-reserved port for P4Runtime.
// TODO(max): Remove the deprecated port 28000 from default list.
constexpr char kExternalStratumUrls[] =
    "0.0.0.0:28000,0.0.0.0:9339,0.0.0.0:9559";

// Default URLs for the Sandcastle services Stratum service will connect to
// over gRPC.
constexpr char kProcmonServiceUrl[] = "localhost:28001";
constexpr char kHalServiceUrl[] = "localhost:28002";
constexpr char kPhalDbServiceUrl[] = "localhost:28003";

// Rexgex pattern for mac address in openconfig-yang-types.yang
constexpr char kMacAddressRegex[] = "^[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}$";
}  // namespace stratum

#endif  // STRATUM_LIB_CONSTANTS_H_
