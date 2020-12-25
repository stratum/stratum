// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/glue/status/status.h"

#include <stdio.h>

#include <algorithm>

#include "absl/strings/str_cat.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  InitGoogle(argv[0], &argc, &argv, true);
#ifdef BENCHMARK
  RunSpecifiedBenchmarks();
#endif  // BENCHMARK
  return RUN_ALL_TESTS();
}

class MyErrorSpace : public util::ErrorSpace {
 public:
  explicit MyErrorSpace(const char* name) : util::ErrorSpace(name) {}
  std::string String(int code) const {
    return absl::StrCat("error(", code, "%d)");
  }

  ::absl::StatusCode CanonicalCode(const ::util::Status& status) const {
    switch (status.error_code()) {
      case 60:
        return ::absl::StatusCode::kPermissionDenied;
      default:
        return ::absl::StatusCode::kUnknown;
    }
  }
};
static MyErrorSpace my_error_space("myerrors");
static MyErrorSpace my_error_space2("myerrors2");

// Typed null pointer for tests below
static const util::ErrorSpace* kNullSpace =
    reinterpret_cast<util::ErrorSpace*>(NULL);

static const util::ErrorSpace* OkSpace() {
  return ::util::Status::OK.error_space();
}

static const ::absl::StatusCode CanonicalCode(const ::util::Status& s) {
  return s.ToCanonical().CanonicalCode();
}

// Check that s has the specified fields.
// An empty payload means the s must not contain a payload.
static void CheckStatus(const ::util::Status& s, const util::ErrorSpace* space,
                        int code, const std::string& message) {
  EXPECT_EQ(code, s.error_code()) << s;
  EXPECT_EQ(space, s.error_space()) << s;
  EXPECT_EQ(message, s.error_message()) << s;

  if (code == 0) {
    EXPECT_TRUE(s.ok()) << s;
    EXPECT_EQ(::absl::StatusCode::kOk, CanonicalCode(s));
    CHECK_EQ("OK", s.ToString()) << s;
  } else {
    EXPECT_TRUE(!s.ok()) << s;
    EXPECT_EQ(::absl::StatusCode::kUnknown, CanonicalCode(s));
    EXPECT_THAT(s.ToString(), testing::HasSubstr(space->SpaceName()));
    EXPECT_THAT(s.ToString(), testing::HasSubstr(space->String(code)));
    EXPECT_THAT(s.ToString(), testing::HasSubstr(message));
  }
}

TEST(ErrorSpace, SpaceName) {
  ASSERT_EQ(std::string("generic"),
            ::util::Status::OK.error_space()->SpaceName());
  ASSERT_EQ(std::string("myerrors"), my_error_space.SpaceName());
}

TEST(ErrorSpace, FindKnown) {
  ASSERT_EQ(&my_error_space, util::ErrorSpace::Find("myerrors"));
  ASSERT_EQ(&my_error_space2, util::ErrorSpace::Find("myerrors2"));
}

TEST(ErrorSpace, FindGeneric) {
  ASSERT_NE(kNullSpace, util::ErrorSpace::Find("generic"));
}

TEST(ErrorSpace, FindUnknown) {
  ASSERT_EQ(kNullSpace, util::ErrorSpace::Find("nonexistent_error_space"));
}

TEST(ErrorSpace, FindDestroyed) {
  { MyErrorSpace temp_space("temporary_name"); }
  ASSERT_EQ(kNullSpace, util::ErrorSpace::Find("temporary_name"));
}

TEST(ErrorSpace, GenericCodeNames) {
  const util::ErrorSpace* e = ::util::Status::CANCELLED.error_space();
  EXPECT_EQ("OK", e->String(static_cast<int>(::absl::StatusCode::kOk)));
  EXPECT_EQ("cancelled", e->String(static_cast<int>(::absl::StatusCode::kCancelled)));
  EXPECT_EQ("unknown", e->String(static_cast<int>(::absl::StatusCode::kUnknown)));
  EXPECT_EQ("aborted", e->String(static_cast<int>(::absl::StatusCode::kAborted)));
  EXPECT_EQ("1000", e->String(1000));  // Out of range
}

TEST(Status, Empty) {
  ::util::Status status;
  CheckStatus(status, OkSpace(), 0, "");
}

TEST(Status, OK) { CheckStatus(::util::Status::OK, OkSpace(), 0, ""); }

TEST(Status, GenericCodes) {
  EXPECT_EQ(static_cast<int>(::absl::StatusCode::kOk),
            static_cast<int>(::util::Status::OK_CODE));
  EXPECT_EQ(static_cast<int>(::absl::StatusCode::kCancelled),
            static_cast<int>(::util::Status::CANCELLED_CODE));
  EXPECT_EQ(static_cast<int>(::absl::StatusCode::kUnknown),
            static_cast<int>(::util::Status::UNKNOWN_CODE));
}

TEST(Status, ConstructorZero) {
  ::util::Status status(&my_error_space, 0, "msg");
  CheckStatus(status, OkSpace(), 0, "");
}

TEST(Status, CheckOK) {
  ::util::Status status;
  CHECK_OK(status);
  CHECK_OK(status) << "Failed";
  DCHECK_OK(status) << "Failed";
}

TEST(DeathStatus, CheckOK) {
  ::util::Status status;
  status =
      ::util::Status(::util::Status::canonical_space(),
                     ::util::Status::CANCELLED_CODE, "Operation Cancelled");
  ASSERT_DEATH(CHECK_OK(status), "Operation Cancelled");
}

TEST(Status, SetErrorZero) {
  ::util::Status status(&my_error_space, 2, "message");
  status = ::util::Status(&my_error_space, 0, "msg");
  CheckStatus(status, OkSpace(), 0, "");
}

TEST(Status, Cancelled) {
  ASSERT_THAT(::util::Status::CANCELLED.ToString(),
              testing::HasSubstr("cancel"));
}

TEST(Status, Filled) {
  ::util::Status status(&my_error_space, 2, "message");
  CheckStatus(status, &my_error_space, 2, "message");
}

TEST(Status, FilledNegative) {
  ::util::Status status(&my_error_space, -2, "message");
  CheckStatus(status, &my_error_space, -2, "message");
}

TEST(Status, Set) {
  ::util::Status status;
  status = ::util::Status(&my_error_space, 2, "message");
  CheckStatus(status, &my_error_space, 2, "message");
}

TEST(Status, SetOverlappingMessage) {
  ::util::Status status;
  status = ::util::Status(&my_error_space, 2, "message");
  CheckStatus(status, &my_error_space, 2, "message");

  std::string old_message = status.error_message();
  status = ::util::Status(&my_error_space, 2, old_message);
  CheckStatus(status, &my_error_space, 2, "message");

  std::string full_message = status.error_message();
  std::string part_message = full_message.substr(1, 3);
  EXPECT_EQ(part_message, "ess");
  status = ::util::Status(&my_error_space, 2, part_message);
  CheckStatus(status, &my_error_space, 2, "ess");
}

TEST(Status, Clear) {
  ::util::Status status(&my_error_space, 2, "message");
  status.Clear();
  CheckStatus(status, OkSpace(), 0, "");
}

TEST(Status, Copy) {
  ::util::Status a(&my_error_space, 2, "message");
  ::util::Status b(a);
  ASSERT_EQ(a.ToString(), b.ToString());
}

TEST(Status, Assign) {
  ::util::Status a(&my_error_space, 2, "message");
  ::util::Status b;
  b = a;
  ASSERT_EQ(a.ToString(), b.ToString());
}

TEST(Status, Update) {
  ::util::Status s;
  s.Update(::util::Status::OK);
  ASSERT_TRUE(s.ok());
  ::util::Status a(&my_error_space, 2, "message");
  s.Update(a);
  ASSERT_EQ(s.ToString(), a.ToString());
  ::util::Status b(&my_error_space, 17, "other message");
  s.Update(b);
  ASSERT_EQ(s.ToString(), a.ToString());
  s.Update(::util::Status::OK);
  ASSERT_EQ(s.ToString(), a.ToString());
  ASSERT_FALSE(s.ok());
}

TEST(Status, Swap) {
  ::util::Status a(&my_error_space, 2, "message");
  ::util::Status b = a;
  ::util::Status c;
  c.Swap(&a);
  ASSERT_EQ(c.ToString(), b.ToString());
  ASSERT_EQ(a.ToString(), ::util::Status::OK.ToString());
}

TEST(Status, UnknownCode) {
  ::util::Status status(&my_error_space, 10, "message");
  ASSERT_TRUE(!status.ok());
  ASSERT_EQ(10, status.error_code());
  ASSERT_EQ(::absl::StatusCode::kUnknown, CanonicalCode(status));
  ASSERT_EQ(std::string("message"), status.error_message());
  ASSERT_EQ(status.error_space(), &my_error_space);
  ASSERT_THAT(status.ToString(),
              testing::MatchesRegex("myerrors.*10.*message"));
}

TEST(Status, MatchOK) {
  ASSERT_TRUE(::util::Status().Matches(::util::Status::OK));
}

TEST(Status, MatchSame) {
  const ::util::Status a = ::util::Status(&my_error_space, 1, "message");
  const ::util::Status b = ::util::Status(&my_error_space, 1, "message");
  ASSERT_TRUE(a.Matches(b));
}

TEST(Status, MatchCopy) {
  const ::util::Status a = ::util::Status(&my_error_space, 1, "message");
  const ::util::Status b = a;
  ASSERT_TRUE(a.Matches(b));
}

TEST(Status, MatchDifferentCode) {
  const ::util::Status a = ::util::Status(&my_error_space, 1, "message");
  const ::util::Status b = ::util::Status(&my_error_space, 2, "message");
  ASSERT_TRUE(!a.Matches(b));
}

TEST(Status, MatchDifferentSpace) {
  const ::util::Status a = ::util::Status(&my_error_space, 1, "message");
  const ::util::Status b = ::util::Status(&my_error_space2, 1, "message");
  ASSERT_TRUE(!a.Matches(b));
}

TEST(Status, MatchDifferentMessage) {
  const ::util::Status a = ::util::Status(&my_error_space, 1, "message");
  const ::util::Status b = ::util::Status(&my_error_space, 1, "another");
  ASSERT_TRUE(a.Matches(b));
}

TEST(Status, EqualsOK) { ASSERT_EQ(::util::Status::OK, ::util::Status()); }

TEST(Status, EqualsSame) {
  const ::util::Status a = ::util::Status(&my_error_space, 1, "message");
  const ::util::Status b = ::util::Status(&my_error_space, 1, "message");
  ASSERT_EQ(a, b);
}

TEST(Status, EqualsCopy) {
  const ::util::Status a = ::util::Status(&my_error_space, 1, "message");
  const ::util::Status b = a;
  ASSERT_EQ(a, b);
}

TEST(Status, EqualsDifferentCode) {
  const ::util::Status a = ::util::Status(&my_error_space, 1, "message");
  const ::util::Status b = ::util::Status(&my_error_space, 2, "message");
  ASSERT_NE(a, b);
}

TEST(Status, EqualsDifferentSpace) {
  const ::util::Status a = ::util::Status(&my_error_space, 1, "message");
  const ::util::Status b = ::util::Status(&my_error_space2, 1, "message");
  ASSERT_NE(a, b);
}

TEST(Status, EqualsDifferentMessage) {
  const ::util::Status a = ::util::Status(&my_error_space, 1, "message");
  const ::util::Status b = ::util::Status(&my_error_space, 1, "another");
  ASSERT_NE(a, b);
}

TEST(Status, EqualsCanonicalCodeSame) {
  ::util::Status a = ::util::Status(&my_error_space, 1234, "message");
  ::util::Status b = ::util::Status(&my_error_space, 1234, "message");
  ASSERT_EQ(a, b);
  a.SetCanonicalCode(::absl::StatusCode::kResourceExhausted);
  b.SetCanonicalCode(::absl::StatusCode::kResourceExhausted);
  ASSERT_EQ(a, b);
}

TEST(Status, EqualsCanonicalCodeMismatch) {
  ::util::Status a = ::util::Status(&my_error_space, 1234, "message");
  ::util::Status b = ::util::Status(&my_error_space, 1234, "message");
  ASSERT_EQ(a, b);
  a.SetCanonicalCode(::absl::StatusCode::kResourceExhausted);
  b.SetCanonicalCode(::absl::StatusCode::kUnavailable);
  ASSERT_NE(a, b);
}

TEST(Status, StripMessage) {
  ::util::Status a = ::util::Status(&my_error_space, 1, "");
  ::util::Status b = ::util::Status(&my_error_space, 1, "x");
  ASSERT_EQ(a, b.StripMessage());
}

static void SanityCheck(const ::util::Status& s, const util::ErrorSpace* space,
                        int code, const std::string& msg) {
  EXPECT_EQ(code, s.error_code());
  EXPECT_EQ(space, s.error_space());

  ::util::Status copy(s);
  EXPECT_EQ(s, copy);

  ::util::Status other(::absl::StatusCode::kDeadlineExceeded, "_sanity_check_");
  EXPECT_NE(other, s);

  ::util::Status updated;
  updated.Update(s);
  EXPECT_EQ(s, updated);

  // Matches / StripMessage
  ::util::Status with_msg(space, code, "_sanity_check_");
  EXPECT_TRUE(s.Matches(with_msg));
  EXPECT_EQ(s, with_msg.StripMessage());
  if (!s.ok()) {
    EXPECT_FALSE(s == with_msg);
  }

  // SetError
  ::util::Status err;
  err = ::util::Status(space, code, msg);
  EXPECT_EQ(s, err);
}

TEST(Status, Globals) {
  const util::ErrorSpace* space = ::util::Status::canonical_space();
  SanityCheck(::util::Status::OK, space, ::util::Status::OK_CODE, "");
  SanityCheck(::util::Status::CANCELLED, space, ::util::Status::CANCELLED_CODE,
              "");
  SanityCheck(::util::Status::UNKNOWN, space, ::util::Status::UNKNOWN_CODE, "");
}

TEST(Canonical, WrongSpace) {
  ::util::Status status(&my_error_space, 1, "message");
  const util::ErrorSpace* space = ::util::Status::canonical_space();
  EXPECT_EQ(::absl::StatusCode::kUnknown, space->CanonicalCode(status));
}

TEST(Canonical, CustomMapping) {
  ::util::Status s(&my_error_space, 60, "message");
  EXPECT_EQ(::absl::StatusCode::kPermissionDenied, CanonicalCode(s));
}

static void VerifyCanonical(const ::util::Status& s,
                            ::absl::StatusCode match_code,
                            ::absl::StatusCode nomatch_code) {
  EXPECT_EQ(match_code, s.CanonicalCode());
  EXPECT_TRUE(s.Matches(match_code)) << match_code;
  EXPECT_FALSE(s.Matches(nomatch_code)) << nomatch_code;
}

TEST(Canonical, CanonicalCode) {
  ::util::Status ok = ::util::Status::OK;
  ::util::Status cancel = ::util::Status::CANCELLED;
  ::util::Status perm(&my_error_space, 60, "message");
  ::util::Status other(&my_error_space, 10, "message");
  VerifyCanonical(ok, ::absl::StatusCode::kOk, ::absl::StatusCode::kUnknown);
  VerifyCanonical(cancel, ::absl::StatusCode::kCancelled, ::absl::StatusCode::kUnknown);
  VerifyCanonical(perm, ::absl::StatusCode::kPermissionDenied,
                  ::absl::StatusCode::kUnknown);
  VerifyCanonical(other, ::absl::StatusCode::kUnknown,
                  ::absl::StatusCode::kPermissionDenied);

  // Check handling of a canonical code not known in this address space.
  perm.SetCanonicalCode(::absl::StatusCode::kUnknown);
  VerifyCanonical(perm, ::absl::StatusCode::kUnknown,
                  ::absl::StatusCode::kPermissionDenied);
}

TEST(Canonical, SetCanonicalCode) {
  ::util::Status s(&my_error_space, 1234, "message");
  s.SetCanonicalCode(::absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(1234, s.error_code());
  EXPECT_EQ(::absl::StatusCode::kResourceExhausted, CanonicalCode(s));
}

TEST(Canonical, SetCanonicalCodeIgnoredOnOkStatus) {
  ::util::Status s(&my_error_space, 0, "message");
  s.SetCanonicalCode(::absl::StatusCode::kResourceExhausted);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(::absl::StatusCode::kOk, CanonicalCode(s));
}

TEST(Canonical, SetCanonicalCodeIgnoredOnCanonicalSpace) {
  ::util::Status s(::absl::StatusCode::kDeadlineExceeded, "message");
  s.SetCanonicalCode(::absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(::absl::StatusCode::kDeadlineExceeded, CanonicalCode(s));
}

TEST(Canonical, SetCanonicalCodeOnSharedStatus) {
  const ::util::Status x(&my_error_space, 1234, "message");

  // Set canonical code on a copy.
  ::util::Status y = x;
  y.SetCanonicalCode(::absl::StatusCode::kResourceExhausted);
  EXPECT_NE(x, y);
  EXPECT_EQ(x.error_space(), y.error_space());
  EXPECT_EQ(x.error_code(), y.error_code());
  EXPECT_EQ(x.error_message(), y.error_message());
  EXPECT_EQ(::absl::StatusCode::kUnknown, CanonicalCode(x));
  EXPECT_EQ(::absl::StatusCode::kResourceExhausted, CanonicalCode(y));

  // Yet another copy, with a different code set.
  ::util::Status z = y;
  z.SetCanonicalCode(::absl::StatusCode::kDeadlineExceeded);
  EXPECT_NE(y, z);
  EXPECT_EQ(x.error_space(), z.error_space());
  EXPECT_EQ(x.error_code(), z.error_code());
  EXPECT_EQ(x.error_message(), z.error_message());
  EXPECT_EQ(::absl::StatusCode::kResourceExhausted, CanonicalCode(y));
  EXPECT_EQ(::absl::StatusCode::kDeadlineExceeded, CanonicalCode(z));
}

#ifdef BENCHMARK
static void BM_StatusCreateDestroy(int iters) {
  int count = 0;
  ::util::Status dummy = ::util::Status::CANCELLED;
  for (int i = 0; i < iters; i++) {
    ::util::Status s;
    if (i == 17) {
      s = dummy;
    }
    if (!s.ok()) {
      count++;
    }
  }
  if (count == -1) {
    fprintf(stderr, "Dummy use");
  }
}
BENCHMARK(BM_StatusCreateDestroy);

static void BM_StatusCopy(int iters) {
  ::util::Status dummy[2];
  dummy[1] = ::util::Status::CANCELLED;
  int count = 0;
  for (int i = 0; i < iters; i++) {
    ::util::Status s = dummy[i == 17];
    if (!s.ok()) {
      count++;
    }
  }
  if (count == -1) {
    fprintf(stderr, "Dummy use");
  }
}
BENCHMARK(BM_StatusCopy);
#endif  // BENCHMARK
