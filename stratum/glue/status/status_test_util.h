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

#include "third_party/stratum/glue/status/status.h"
#include "testing/base/public/gunit.h"

// Macros for testing the results of functions that return ::util::Status.

#define EXPECT_OK(statement) EXPECT_EQ(::util::Status::OK, (statement))
#define ASSERT_OK(statement) ASSERT_EQ(::util::Status::OK, (statement))

// There are no EXPECT_NOT_OK/ASSERT_NOT_OK macros since they would not
// provide much value (when they fail, they would just print the OK status
// which conveys no more information than EXPECT_FALSE(status.ok());
// If you want to check for particular errors, better alternatives are:
// EXPECT_EQ(::util::Status(...expected error...), status.StripMessage());
// EXPECT_THAT(status.ToString(), HasSubstr("expected error"));
// Also, see testing/base/public/gmock.h.

#endif  // STRATUM_GLUE_STATUS_STATUS_TEST_UTIL_H_
