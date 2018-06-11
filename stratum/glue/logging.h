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


#ifndef STRATUM_GLUE_LOGGING_H_
#define STRATUM_GLUE_LOGGING_H_

#include "gflags/gflags.h"
#include "glog/logging.h"  // IWYU pragma: export

#ifdef STRATUM_ARCH_PPC

// This flag exists in normal glog, but is not included in portable base.
DECLARE_bool(logtostderr);

#endif  // STRATUM_ARCH_PPC

DECLARE_bool(logtosyslog);


// These are exported in open source glog but not base/logging.h
using google::ERROR;
using google::FATAL;
using google::INFO;
using google::WARNING;
#define LOG_EXT(facility, level) LOG(level)

namespace stratum {
// Initializes all Hercules specific changes to logging. This should be called
// after InitGoogle by every Hercules binary.
void InitHerculesLogging();
}  // namespace stratum

#endif  // STRATUM_GLUE_LOGGING_H_
