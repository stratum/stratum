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

// This is the main entry for common lib modules tests.
#include <stdlib.h>

#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"

DEFINE_string(test_tmpdir, "", "Temp directory to be used for tests.");

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  InitGoogle(argv[0], &argc, &argv, true);

  bool tmpdir_created = false;
  if (FLAGS_test_tmpdir.empty()) {
    char tmpdir[] = "/tmp/stratum_lib_test.XXXXXX";
    CHECK(mkdtemp(tmpdir));
    FLAGS_test_tmpdir = tmpdir;
    tmpdir_created = true;
    LOG(INFO) << "Created FLAGS_test_tmpdir " << FLAGS_test_tmpdir;
  }

  int result = RUN_ALL_TESTS();

  if (tmpdir_created) {
    const std::string cleanup("rm -rf " + FLAGS_test_tmpdir);
    system(cleanup.c_str());
    LOG(INFO) << "Cleaned up FLAGS_test_tmpdir " << FLAGS_test_tmpdir;
  }

  return result;
}
