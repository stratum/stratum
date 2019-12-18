// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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

#include "stratum/glue/logging.h"

#include <syslog.h>
#include <memory>

#include "gflags/gflags.h"

#ifdef STRATUM_ARCH_PPC

DEFINE_bool(logtostderr, false,
            "log messages go to stderr instead of logfiles.");

#endif  // STRATUM_ARCH_PPC

DEFINE_bool(logtosyslog, false, "log messages also go to syslog.");

using google::LogSeverity;
using google::LogToStderr;
using google::ProgramInvocationShortName;
using google::LogSink;

namespace stratum {
namespace {
// This LogSink outputs every log message to syslog.
class SyslogSink : public LogSink {
 public:
  void send(LogSeverity severity, const char* full_filename,
                    const char* base_filename, int line,
                    const struct ::tm* tm_time,
                    const char* message, size_t message_len) override {
    static const int kSeverityToLevel[] = {INFO, WARNING, ERROR, FATAL};
    static const char* const kSeverityToLabel[] = {"INFO", "WARNING", "ERROR",
                                                   "FATAL"};
    int severity_int = static_cast<int>(severity);
    syslog(LOG_USER | kSeverityToLevel[severity_int], "%s %s:%d] %.*s",
           kSeverityToLabel[severity_int], base_filename, line,
           static_cast<int>(message_len), message);
  }
};
}  // namespace

void InitStratumLogging() {
  google::InitGoogleLogging(ProgramInvocationShortName());
  google::InstallFailureSignalHandler();
  // Make sure we only setup log_sink once.
  static SyslogSink* log_sink = nullptr;
  if (FLAGS_logtosyslog && log_sink == nullptr) {
    openlog(ProgramInvocationShortName(), LOG_CONS | LOG_PID | LOG_NDELAY,
            LOG_USER);
    log_sink = new SyslogSink();
    AddLogSink(log_sink);
  }
  if (FLAGS_logtostderr) {
    LogToStderr();
  }
}

}  // namespace stratum

// ostream overload for std::nulptr_t for C++11
// see: https://stackoverflow.com/a/46256849
#if __cplusplus == 201103L
#include <cstddef>
#include <iostream>
namespace std {
::std::ostream& operator<<(::std::ostream& s, ::std::nullptr_t) {
    return s << static_cast<void *>(nullptr);
}
}
#endif  // __cplusplus
