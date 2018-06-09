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


#include "stratum/lib/utils.h"

#include "gflags/gflags.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/public/lib/error.h"
#include "stratum/public/proto/hal.grpc.pb.h"
#include "testing/base/public/gunit.h"
#include "sandblaze/p4lang/p4/p4runtime.grpc.pb.h"

DECLARE_string(test_tmpdir);

namespace stratum {

TEST(CommonUtilsTest, PrintArrayForEmptyArray) {
  int iarray[] = {};
  std::string sarray[] = {};
  EXPECT_EQ("()", PrintArray(iarray, 0, ", "));
  EXPECT_EQ("()", PrintArray(sarray, 0, ", "));
}

TEST(CommonUtilsTest, PrintArrayForNonEmptyArray) {
  int iarray[] = {1, 3, 5};
  std::string sarray[] = {"test1", "test2"};
  double darray[] = {1.234};
  EXPECT_EQ("(1, 3, 5)", PrintArray(iarray, 3, ", "));
  EXPECT_EQ("(test1; test2)", PrintArray(sarray, 2, "; "));
  EXPECT_EQ("(1.234)", PrintArray(darray, 1, ", "));
}

TEST(CommonUtilsTest, PrintVectorForEmptyVector) {
  std::vector<int> ivector = {};
  std::vector<std::string> svector = {};
  EXPECT_EQ("()", PrintVector(ivector, ", "));
  EXPECT_EQ("()", PrintVector(svector, ", "));
}

TEST(CommonUtilsTest, PrintVectorForNonEmptyVector) {
  std::vector<int> ivector = {1, 3, 5};
  std::vector<std::string> svector = {"test1", "test2"};
  std::vector<double> dvector = {1.234};
  EXPECT_EQ("(1, 3, 5)", PrintVector(ivector, ", "));
  EXPECT_EQ("(test1; test2)", PrintVector(svector, "; "));
  EXPECT_EQ("(1.234)", PrintVector(dvector, ", "));
}

TEST(CommonUtilsTest, WriteProtoToBinFileThenReadProtoFromBinFile) {
  ChassisConfig expected, actual;
  expected.set_description("Test config");
  expected.mutable_chassis()->set_platform(PLT_GENERIC_TOMAHAWK);
  expected.add_nodes()->set_id(1);
  expected.add_nodes()->set_id(2);
  const std::string filename(FLAGS_test_tmpdir +
                             "/WriteProtoToBinFileThenReadProtoFromBinFile");
  ASSERT_OK(WriteProtoToBinFile(expected, filename));
  ASSERT_OK(ReadProtoFromBinFile(filename, &actual));
  EXPECT_TRUE(ProtoEqual(expected, actual));
}

TEST(CommonUtilsTest, WriteProtoToTextFileThenReadProtoFromTextFile) {
  ChassisConfig expected, actual;
  expected.set_description("Test config");
  expected.mutable_chassis()->set_platform(PLT_GENERIC_TOMAHAWK);
  expected.add_nodes()->set_id(1);
  expected.add_nodes()->set_id(2);
  expected.add_nodes()->set_id(5);
  const std::string filename(FLAGS_test_tmpdir +
                             "/WriteProtoToTextFileThenReadProtoFromTextFile");
  ASSERT_OK(WriteProtoToTextFile(expected, filename));
  ASSERT_OK(ReadProtoFromTextFile(filename, &actual));
  EXPECT_TRUE(ProtoEqual(expected, actual));
}

TEST(CommonUtilsTest, PrintProtoToStringThenParseProtoFromString) {
  ChassisConfig expected, actual;
  expected.set_description("Test config");
  expected.mutable_chassis()->set_platform(PLT_GENERIC_TOMAHAWK);
  expected.add_nodes()->set_id(1);
  expected.add_nodes()->set_id(2);
  std::string text = "";
  ASSERT_OK(PrintProtoToString(expected, &text));
  ASSERT_OK(ParseProtoFromString(text, &actual));
  EXPECT_TRUE(ProtoEqual(expected, actual));
}

TEST(CommonUtilsTest, WriteStringToFileThenReadFileToString) {
  std::string expected(
      "Lorem ipsum dolor sit amet, consectetur "
      "elit, sed do eiusmod tempor incididunt ut labore "
      "dolore magna aliqua.");
  std::string actual = "";
  const std::string filename(FLAGS_test_tmpdir +
                             "/WriteStringToFileThenReadFileToString");
  ASSERT_OK(WriteStringToFile(expected, filename));
  ASSERT_OK(ReadFileToString(filename, &actual));
  EXPECT_EQ(expected, actual);
  expected += "some more data";
  ASSERT_OK(WriteStringToFile("some more data", filename, /*append=*/true));
  actual = "";
  ASSERT_OK(ReadFileToString(filename, &actual));
  EXPECT_EQ(expected, actual);
}

TEST(CommonUtilsTest, RecursivelyCreateDirThenCheckThePath) {
  const std::string testdir(FLAGS_test_tmpdir + "/path/to/another/dir");
  ASSERT_TRUE(PathExists(FLAGS_test_tmpdir));
  ASSERT_TRUE(IsDir(FLAGS_test_tmpdir));
  ASSERT_FALSE(PathExists(testdir));
  ASSERT_FALSE(IsDir(testdir));
  // Now create the dir and check that it exists.
  ASSERT_OK(RecursivelyCreateDir(testdir));
  ASSERT_TRUE(PathExists(testdir));
  ASSERT_TRUE(IsDir(testdir));
}

TEST(CommonUtilsTest, ProtoEqualProtoLessProtoHash) {
  ::p4::TableEntry e1, e2;
  const std::string kTableEntryText1 = R"(
      table_id: 12
      match {
        field_id: 1
        lpm {
          prefix_len: 32
          value: "\x01\x02\x03\x04"
        }
      }
      match {
        field_id: 2
        exact {
          value: "\x0a"
        }
      }
      action {
        action_profile_member_id: 1
      }
  )";
  const std::string kTableEntryText2 = R"(
      table_id: 12
      match {
        field_id: 2
        exact {
          value: "\x0a"
        }
      }
      match {
        field_id: 1
        lpm {
          prefix_len: 32
          value: "\x01\x02\x03\x04"
        }
      }
      action {
        action_profile_member_id: 1
      }
  )";
  // Before sorting the repeated fields, the protos are equal but their hashes
  // are not
  ASSERT_OK(ParseProtoFromString(kTableEntryText1, &e1));
  ASSERT_OK(ParseProtoFromString(kTableEntryText2, &e2));
  EXPECT_TRUE(ProtoEqual(e1, e2));
  EXPECT_NE(ProtoHash(e1), ProtoHash(e2));
  // After sorting the repeated fields, we expect the protos to be equal and
  // hash to the same value.
  std::sort(e1.mutable_match()->begin(), e1.mutable_match()->end(),
            [](const ::p4::FieldMatch& l, const ::p4::FieldMatch& r) {
              return ProtoLess(l, r);
            });
  std::sort(e2.mutable_match()->begin(), e2.mutable_match()->end(),
            [](const ::p4::FieldMatch& l, const ::p4::FieldMatch& r) {
              return ProtoLess(l, r);
            });
  EXPECT_TRUE(ProtoEqual(e1, e2));
  EXPECT_EQ(ProtoHash(e1), ProtoHash(e2));
}

TEST(CommonUtilsTest, ByteStreamToUint16) {
  const std::string kTestBytes1 = {0xab};
  uint16 value = ByteStreamToUint<uint16>(kTestBytes1);
  EXPECT_EQ(0xab, value);
  const std::string kTestBytes2 = {0xab, 0xcd};
  value = ByteStreamToUint<uint16>(kTestBytes2);
  EXPECT_EQ(0xabcd, value);
}

TEST(CommonUtilsTest, ByteStreamToUint32) {
  const std::string kTestBytes1 = {0xab};
  uint32 value = ByteStreamToUint<uint32>(kTestBytes1);
  EXPECT_EQ(0xab, value);
  const std::string kTestBytes2 = {0xab, 0xcd};
  value = ByteStreamToUint<uint32>(kTestBytes2);
  EXPECT_EQ(0xabcd, value);
  const std::string kTestBytes3 = {0xab, 0xcd, 0xef};
  value = ByteStreamToUint<uint32>(kTestBytes3);
  EXPECT_EQ(0xabcdef, value);
  const std::string kTestBytes4 = {0x89, 0xab, 0xcd, 0xef};
  value = ByteStreamToUint<uint32>(kTestBytes4);
  EXPECT_EQ(0x89abcdef, value);
}

TEST(CommonUtilsTest, ByteStreamToUint64) {
  const std::string kTestBytes1 = {0xab};
  uint64 value = ByteStreamToUint<uint64>(kTestBytes1);
  EXPECT_EQ(0xab, value);
  const std::string kTestBytes2 = {0xab, 0xcd};
  value = ByteStreamToUint<uint64>(kTestBytes2);
  EXPECT_EQ(0xabcd, value);
  const std::string kTestBytes3 = {0xab, 0xcd, 0xef};
  value = ByteStreamToUint<uint64>(kTestBytes3);
  EXPECT_EQ(0xabcdef, value);
  const std::string kTestBytes4 = {0x89, 0xab, 0xcd, 0xef};
  value = ByteStreamToUint<uint64>(kTestBytes4);
  EXPECT_EQ(0x89abcdef, value);
  const std::string kTestBytes5 = {0x67, 0x89, 0xab, 0xcd, 0xef};
  value = ByteStreamToUint<uint64>(kTestBytes5);
  EXPECT_EQ(0x6789abcdefULL, value);
  const std::string kTestBytes6 = {0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  value = ByteStreamToUint<uint64>(kTestBytes6);
  EXPECT_EQ(0x456789abcdefULL, value);
  const std::string kTestBytes7 = {0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  value = ByteStreamToUint<uint64>(kTestBytes7);
  EXPECT_EQ(0x23456789abcdefULL, value);
  const std::string kTestBytes8 = {1, 2, 3, 4, 5, 6, 7, 8};
  value = ByteStreamToUint<uint64>(kTestBytes8);
  EXPECT_EQ(0x0102030405060708ULL, value);
}

TEST(CommonUtilsTest, ByteStreamTruncate) {
  std::string bytes_too_long = {0x01, 0xab, 0xcd};
  uint16 value16 = ByteStreamToUint<uint16>(bytes_too_long);
  EXPECT_EQ(0x01ab, value16);
  bytes_too_long = {0x01, 0x89, 0xab, 0xcd, 0xef};
  uint32 value32 = ByteStreamToUint<uint32>(bytes_too_long);
  EXPECT_EQ(0x0189abcd, value32);
  bytes_too_long = {0xf, 1, 2, 3, 4, 5, 6, 7, 8};
  uint64 value64 = ByteStreamToUint<uint64>(bytes_too_long);
  EXPECT_EQ(0x0f01020304050607ULL, value64);
}

}  // namespace stratum
