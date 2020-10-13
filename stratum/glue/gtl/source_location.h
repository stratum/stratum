// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_GLUE_GTL_SOURCE_LOCATION_H_
#define STRATUM_GLUE_GTL_SOURCE_LOCATION_H_

#include <string>

namespace stratum {
namespace gtl {

class source_location {
 public:
  source_location(const std::string file, const int line)
      : file_(file), line_(line) {}
  std::string file_name() const { return file_; }
  int line() const { return line_; }

 private:
  const std::string file_;
  const int line_;
};

}  // namespace gtl
}  // namespace stratum

#define GTL_LOC stratum::gtl::source_location(__FILE__, __LINE__)

#endif  // STRATUM_GLUE_GTL_SOURCE_LOCATION_H_
