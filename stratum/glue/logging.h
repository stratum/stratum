// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_GLUE_LOGGING_H_
#define STRATUM_GLUE_LOGGING_H_

#include "gflags/gflags.h"

// If for some reason ERROR gets defined somewhere, glog will not compile
#ifdef ERROR
#undef ERROR
#endif
#include "glog/logging.h"  // IWYU pragma: export

#ifdef STRATUM_ARCH_PPC

// This flag exists in normal glog, but is not included in portable base.
DECLARE_bool(logtostderr);

#endif  // STRATUM_ARCH_PPC

DECLARE_bool(logtosyslog);


// These are exported in open source glog but not base/logging.h
using ::google::ERROR;
using ::google::FATAL;
using ::google::INFO;
using ::google::WARNING;
#define LOG_EXT(facility, level) LOG(level)

namespace stratum {
// Initializes all Stratum specific changes to logging. This should be called
// after InitGoogle by every Stratum binary.
void InitStratumLogging();
}  // namespace stratum

// ostream overload for std::nulptr_t for C++11
// see: https://stackoverflow.com/a/46256849
#if __cplusplus == 201103L
#include <cstddef>
#include <iostream>
namespace std {
::std::ostream& operator<<(::std::ostream& s, ::std::nullptr_t);
}
#endif  // __cplusplus

#endif  // STRATUM_GLUE_LOGGING_H_
