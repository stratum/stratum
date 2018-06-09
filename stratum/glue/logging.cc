// Copyright 2018 Google LLC
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

DEFINE_bool(logtosyslog, true,
            "log messages also go to syslog.");

namespace stratum {
namespace {
// This LogSink outputs every log message to syslog.
class SyslogSink : public base_logging::LogSink {
 public:
  void Send(const absl::LogEntry& e) override {
    static const int kSeverityToLevel[] = {
        base_logging::INFO, base_logging::WARNING, base_logging::ERROR,
        base_logging::FATAL};
    static const char* const kSeverityToLabel[] = {"INFO", "WARNING", "ERROR",
                                                   "FATAL"};
    int severity = static_cast<int>(e.severity);
    syslog(LOG_USER | kSeverityToLevel[severity], "%s %s:%d] %.*s",
           kSeverityToLabel[severity], e.base_filename, e.line,
           static_cast<int>(e.message_len), e.message);
  }
};
}  // namespace

void InitHerculesLogging() {
  // Make sure we only setup log_sink once.
  static SyslogSink* log_sink = nullptr;
  if (FLAGS_logtosyslog && log_sink == nullptr) {
    openlog(base::ProgramInvocationShortName(), LOG_CONS | LOG_PID | LOG_NDELAY,
            LOG_USER);
    log_sink = new SyslogSink();
    AddLogSink(log_sink);
  }
  if (FLAGS_logtostderr) {
    LogToStderr();
  }
}

}  // namespace stratum
