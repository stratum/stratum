// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_GLUE_LOGGING_H_
#define STRATUM_GLUE_LOGGING_H_

#include <string>
#include <utility>

#include "gflags/gflags.h"

// P4c lib/log.h already defines the ERROR macro.
// Issue: https://github.com/p4lang/p4c/issues/2523
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

// An alias for the pair of (glog_severity, glog_verbosity).
using LoggingConfig = std::pair<std::string, std::string>;

// Returns the current glog logging configuration.
LoggingConfig GetCurrentLogLevel();

// Sets a new glog log level for the process. Returns true on success.
bool SetLogLevel(const LoggingConfig& logging_config);
}  // namespace stratum

// ostream overload for std::nullptr_t for C++11
// see: https://stackoverflow.com/a/46256849
// On MacOS this seems to be fixed after Mojave.
#if __cplusplus == 201103L && !defined(__APPLE__)
#include <cstddef>
#include <iostream>
namespace std {
::std::ostream& operator<<(::std::ostream& s, ::std::nullptr_t);
}
#endif  // __cplusplus

#endif  // STRATUM_GLUE_LOGGING_H_
