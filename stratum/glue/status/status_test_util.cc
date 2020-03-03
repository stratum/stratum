// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/glue/status/status_test_util.h"

namespace stratum {
namespace test_utils {
namespace matchers_internal {

void StatusIsMatcherCommonImpl::DescribeTo(std::ostream* os) const {
  *os << "is in an error space that ";
  space_matcher_.DescribeTo(os);
  *os << ", has a status code that ";
  code_matcher_.DescribeTo(os);
  *os << ", and has an error message that ";
  message_matcher_.DescribeTo(os);
}

void StatusIsMatcherCommonImpl::DescribeNegationTo(std::ostream* os) const {
  *os << "is in an error space that ";
  space_matcher_.DescribeNegationTo(os);
  *os << ", or has a status code that ";
  code_matcher_.DescribeNegationTo(os);
  *os << ", or has an error message that ";
  message_matcher_.DescribeNegationTo(os);
}

bool StatusIsMatcherCommonImpl::MatchAndExplain(
    const ::util::Status& status,
    ::testing::MatchResultListener* result_listener) const {
  ::testing::StringMatchResultListener inner_listener;
  if (!space_matcher_.MatchAndExplain(status.error_space(), &inner_listener)) {
    *result_listener << (inner_listener.str().empty()
                             ? "whose error space is wrong"
                             : "which is in an error space " +
                                   inner_listener.str());
    return false;
  }

  inner_listener.Clear();
  if (!code_matcher_.MatchAndExplain(status.error_code(), &inner_listener)) {
    *result_listener << (inner_listener.str().empty()
                             ? "whose status code is wrong"
                             : "which has a status code " +
                                   inner_listener.str());
    return false;
  }

  if (!message_matcher_.Matches(status.error_message())) {
    *result_listener << "whose error message is wrong";
    return false;
  }

  return true;
}

}  // namespace matchers_internal
}  // namespace test_utils
}  // namespace stratum
