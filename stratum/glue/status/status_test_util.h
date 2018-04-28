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


#ifndef STRATUM_GLUE_STATUS_STATUS_TEST_UTIL_H_
#define STRATUM_GLUE_STATUS_STATUS_TEST_UTIL_H_

// We have to include status-matchers.h directly because it's not normally
// included in gunit_no_google3.
// NOLINTNEXTLINE(build/deprecated)
#include "testing/base/public/gmock_utils/status-matchers.h"  // IWYU pragma: export

#endif  // STRATUM_GLUE_STATUS_STATUS_TEST_UTIL_H_
