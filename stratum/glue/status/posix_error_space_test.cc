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


#include "stratum/glue/status/posix_error_space.h"

#include <errno.h>
#include <stddef.h>
#include <string>

#include "testing/base/public/gunit.h"

namespace util {

TEST(PosixErrorSpaceTest, TESTSingleton) {
  const ErrorSpace* error_space = PosixErrorSpace();
  EXPECT_TRUE(error_space != NULL);
  EXPECT_EQ(error_space, ErrorSpace::Find(error_space->SpaceName()));
}

TEST(PosixErrorSpaceTest, TESTSetSpaceName) {
  const ErrorSpace* error_space = PosixErrorSpace();
  EXPECT_TRUE(error_space != NULL);
  EXPECT_STREQ(error_space->SpaceName().c_str(), "util::PosixErrorSpace");
}

TEST(PosixErrorSpaceTest, TESTString) {
  const ErrorSpace* space = PosixErrorSpace();
  EXPECT_STREQ("Success", space->String(0).c_str());
  EXPECT_STREQ("Interrupted system call", space->String(EINTR).c_str());
  EXPECT_STREQ("Unknown error 41", space->String(41).c_str());
}

TEST(PosixErrorSpaceTest, TESTMakeStatus) {
  Status status = PosixErrorToStatus(0, "");
  EXPECT_EQ(0, status.error_code());
  EXPECT_STREQ("", status.error_message().c_str());
  EXPECT_STREQ(status.ToString().c_str(), "OK");

  status = PosixErrorToStatus(0, "Message");
  EXPECT_EQ(0, status.error_code());
  EXPECT_STREQ("", status.error_message().c_str());
  EXPECT_STREQ("OK", status.ToString().c_str());

  status = PosixErrorToStatus(EINTR, "");
  EXPECT_EQ(EINTR, status.error_code());
  EXPECT_STREQ("", status.error_message().c_str());
  EXPECT_STREQ("util::PosixErrorSpace",
               status.error_space()->SpaceName().c_str());
  EXPECT_STREQ("util::PosixErrorSpace::Interrupted system call: ",
               status.ToString().c_str());

  status = PosixErrorToStatus(EINTR, "Message");
  EXPECT_EQ(EINTR, status.error_code());
  EXPECT_STREQ("Message", status.error_message().c_str());
  EXPECT_STREQ("util::PosixErrorSpace",
               status.error_space()->SpaceName().c_str());
  EXPECT_STREQ("util::PosixErrorSpace::Interrupted system call: Message",
               status.ToString().c_str());

  // errno value of 41 is known on linux to not be defined.
  status = PosixErrorToStatus(41, "");
  EXPECT_EQ(41, status.error_code());
  EXPECT_STREQ("", status.error_message().c_str());
  EXPECT_STREQ("util::PosixErrorSpace",
               status.error_space()->SpaceName().c_str());
  EXPECT_STREQ("util::PosixErrorSpace::Unknown error 41: ",
               status.ToString().c_str());

  status = PosixErrorToStatus(41, "Message");
  EXPECT_EQ(41, status.error_code());
  EXPECT_STREQ("Message", status.error_message().c_str());
  EXPECT_STREQ("util::PosixErrorSpace",
               status.error_space()->SpaceName().c_str());
  EXPECT_STREQ("util::PosixErrorSpace::Unknown error 41: Message",
               status.ToString().c_str());
}

TEST(PosixErrorSpaceTest, TESTToCanonical) {
  // All OKs are equal.
  Status status = PosixErrorToStatus(0, "OK");
  EXPECT_EQ(::util::Status::OK, status.ToCanonical());

  // Do one conversion with a message embedded.
  status = PosixErrorToStatus(EINVAL, "Canned message");
  EXPECT_EQ(::util::Status(::util::error::INVALID_ARGUMENT, "Canned message"),
            status.ToCanonical());

  // And now we check only the error codes, using one
  // easy / (relatively) obvious mapping for each output
  // code, mostly just for coverage and as a sanity check.
  status = PosixErrorToStatus(EINVAL, "");
  EXPECT_EQ(::util::error::INVALID_ARGUMENT, status.ToCanonical().error_code());

  status = PosixErrorToStatus(ETIMEDOUT, "");
  EXPECT_EQ(::util::error::DEADLINE_EXCEEDED,
            status.ToCanonical().error_code());

  status = PosixErrorToStatus(ENOENT, "");
  EXPECT_EQ(::util::error::NOT_FOUND, status.ToCanonical().error_code());

  status = PosixErrorToStatus(EEXIST, "");
  EXPECT_EQ(::util::error::ALREADY_EXISTS, status.ToCanonical().error_code());

  status = PosixErrorToStatus(EPERM, "");
  EXPECT_EQ(::util::error::PERMISSION_DENIED,
            status.ToCanonical().error_code());

  status = PosixErrorToStatus(ENOTEMPTY, "");
  EXPECT_EQ(::util::error::FAILED_PRECONDITION,
            status.ToCanonical().error_code());

  status = PosixErrorToStatus(ENOSPC, "");
  EXPECT_EQ(::util::error::RESOURCE_EXHAUSTED,
            status.ToCanonical().error_code());

  status = PosixErrorToStatus(EOVERFLOW, "");
  EXPECT_EQ(::util::error::OUT_OF_RANGE, status.ToCanonical().error_code());

  status = PosixErrorToStatus(EPROTONOSUPPORT, "");
  EXPECT_EQ(::util::error::UNIMPLEMENTED, status.ToCanonical().error_code());

  status = PosixErrorToStatus(EAGAIN, "");
  EXPECT_EQ(::util::error::UNAVAILABLE, status.ToCanonical().error_code());

  status = PosixErrorToStatus(EDEADLK, "");
  EXPECT_EQ(::util::error::ABORTED, status.ToCanonical().error_code());

  status = PosixErrorToStatus(ECANCELED, "");
  EXPECT_EQ(::util::error::CANCELLED, status.ToCanonical().error_code());

  status = PosixErrorToStatus(EL2HLT, "");
  EXPECT_EQ(::util::error::UNKNOWN, status.ToCanonical().error_code());
}
}  // namespace util
