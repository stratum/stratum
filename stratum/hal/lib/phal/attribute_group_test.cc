// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include <memory>

#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/attribute_group.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/datasource_mock.h"
#include "stratum/hal/lib/phal/dummy_threadpool.h"
#include "stratum/hal/lib/phal/managed_attribute.h"
#include "stratum/hal/lib/phal/managed_attribute_mock.h"
#include "stratum/hal/lib/phal/test/test.pb.h"
#include "stratum/hal/lib/phal/test_util.h"
#include "stratum/lib/test_utils/matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/integral_types.h"

namespace stratum {
namespace hal {
namespace phal {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::MockFunction;
using ::testing::Return;
using stratum::test_utils::StatusIs;

namespace {

const int32 kInt32TestVal = 10;
const int64 kInt64TestVal = 20;
const uint32 kUInt32TestVal = 30;
const uint64 kUInt64TestVal = 40;
const float kFloatTestVal = 0.5;
const double kDoubleTestVal = 1.5;
const bool kBoolTestVal = true;
const std::string kStringTestVal = "i am the walrus";  // NOLINT(runtime/string)
const google::protobuf::EnumValueDescriptor* kEnumTestVal =
    TopEnum_descriptor()->FindValueByName("TWO");

template <typename T>
T GetTestVal();
template <>
int32 GetTestVal<int32>() {
  return kInt32TestVal;
}
template <>
int64 GetTestVal<int64>() {
  return kInt64TestVal;
}
template <>
uint32 GetTestVal<uint32>() {
  return kUInt32TestVal;
}
template <>
uint64 GetTestVal<uint64>() {
  return kUInt64TestVal;
}
template <>
float GetTestVal<float>() {
  return kFloatTestVal;
}
template <>
double GetTestVal<double>() {
  return kDoubleTestVal;
}
template <>
bool GetTestVal<bool>() {
  return kBoolTestVal;
}
template <>
std::string GetTestVal<std::string>() {
  return kStringTestVal;
}
template <>
const google::protobuf::EnumValueDescriptor*
GetTestVal<const google::protobuf::EnumValueDescriptor*>() {
  return kEnumTestVal;
}

class AttributeGroupTest : public ::testing::Test {
 public:
  AttributeGroupTest() {
    group_ = AttributeGroup::From(TestTop::descriptor());
  }

 protected:
  template <typename T>
  ManagedAttribute* TestAttr() {
    // We need to keep every datasource in scope, but by sticking them in a
    // vector we can avoid cluttering our tests with lots of datasources.
    auto fixed_datasource = FixedDataSource<T>::Make(GetTestVal<T>());
    managed_datasources_.push_back(fixed_datasource);
    return fixed_datasource->GetAttribute();
  }

  std::unique_ptr<AttributeGroup> group_;
  std::vector<std::shared_ptr<DataSource>> managed_datasources_;
};

TEST_F(AttributeGroupTest, CanAddAllBasicAttributes) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
  EXPECT_OK(mutable_group->AddAttribute("int64_val", TestAttr<int64>()));
  EXPECT_OK(mutable_group->AddAttribute("uint32_val", TestAttr<uint32>()));
  EXPECT_OK(mutable_group->AddAttribute("uint64_val", TestAttr<uint64>()));
  EXPECT_OK(mutable_group->AddAttribute("float_val", TestAttr<float>()));
  EXPECT_OK(mutable_group->AddAttribute("double_val", TestAttr<double>()));
  EXPECT_OK(mutable_group->AddAttribute("bool_val", TestAttr<bool>()));
  EXPECT_OK(mutable_group->AddAttribute("string_val", TestAttr<std::string>()));
}

TEST_F(AttributeGroupTest, CanAddEnumAttributes) {
  auto mutable_group = group_->AcquireMutable();
  auto topenum_datasource = FixedEnumDataSource::Make(TopEnum_descriptor(), 0);
  auto subenum_datasource =
      FixedEnumDataSource::Make(TestTop_SubEnum_descriptor(), 0);
  EXPECT_OK(mutable_group->AddAttribute("top_val",
                                        topenum_datasource->GetAttribute()));
  EXPECT_OK(mutable_group->AddAttribute("sub_val",
                                        subenum_datasource->GetAttribute()));
}

TEST_F(AttributeGroupTest, CannotAddWrongEnumAttributes) {
  auto mutable_group = group_->AcquireMutable();
  auto topenum_datasource = FixedEnumDataSource::Make(TopEnum_descriptor(), 0);
  auto subenum_datasource =
      FixedEnumDataSource::Make(TestTop_SubEnum_descriptor(), 0);
  EXPECT_FALSE(
      mutable_group->AddAttribute("top_val", subenum_datasource->GetAttribute())
          .ok());
  EXPECT_FALSE(
      mutable_group->AddAttribute("sub_val", topenum_datasource->GetAttribute())
          .ok());
}

TEST_F(AttributeGroupTest, CannotAddNonEnumToEnum) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_FALSE(mutable_group->AddAttribute("top_val", TestAttr<uint64>()).ok());
}

TEST_F(AttributeGroupTest, CannotAddFakeAttribute) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_FALSE(mutable_group->AddAttribute("fake_val", TestAttr<int32>()).ok());
}

TEST_F(AttributeGroupTest, CannotAddAttributeForAttributeGroup) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_FALSE(
      mutable_group->AddAttribute("single_sub", TestAttr<int32>()).ok());
}

TEST_F(AttributeGroupTest, CannotAddWrongAttribute) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_FALSE(mutable_group->AddAttribute("bool_val", TestAttr<int32>()).ok());
}

TEST_F(AttributeGroupTest, CanOverwriteAttribute) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
  EXPECT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
}

TEST_F(AttributeGroupTest, AddingWrongAttributeRecoverable) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_FALSE(mutable_group->AddAttribute("bool_val", TestAttr<int32>()).ok());
  EXPECT_OK(mutable_group->AddAttribute("bool_val", TestAttr<bool>()));
}

TEST_F(AttributeGroupTest, CannotSwapGroupAndAttribute) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_FALSE(
      mutable_group->AddAttribute("single_sub", TestAttr<int32>()).ok());
  EXPECT_FALSE(mutable_group->AddChildGroup("int32_val").ok());
}

TEST_F(AttributeGroupTest, CanAddAttributeGroup) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_TRUE(mutable_group->AddChildGroup("single_sub").ok());
}

TEST_F(AttributeGroupTest, CannotAddFakeAttributeGroup) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_FALSE(mutable_group->AddChildGroup("fake_sub").ok());
}

TEST_F(AttributeGroupTest, AddingWrongAttributeGroupRecoverable) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_FALSE(mutable_group->AddChildGroup("fake_sub").ok());
  EXPECT_TRUE(mutable_group->AddChildGroup("single_sub").ok());
}

TEST_F(AttributeGroupTest, CannotAddAttributeGroupTwice) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_TRUE(mutable_group->AddChildGroup("single_sub").ok());
  EXPECT_FALSE(mutable_group->AddChildGroup("single_sub").ok());
}

TEST_F(AttributeGroupTest, CanAddRepeatedAttributeGroupManyTimes) {
  auto mutable_group = group_->AcquireMutable();
  for (int i = 0; i < 10; i++)
    EXPECT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
}

TEST_F(AttributeGroupTest, CannotAddSingleGroupToRepeated) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_FALSE(mutable_group->AddChildGroup("repeated_sub").ok());
}

TEST_F(AttributeGroupTest, CannotAddRepeatedGroupToSingle) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_FALSE(mutable_group->AddRepeatedChildGroup("single_sub").ok());
}

TEST_F(AttributeGroupTest, CannotAddRepeatedGroupToAttribute) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_FALSE(mutable_group->AddRepeatedChildGroup("int32_val").ok());
}

TEST_F(AttributeGroupTest, CanAddAttributeToChild) {
  auto mutable_group = group_->AcquireMutable();
  auto child = mutable_group->AddChildGroup("single_sub");
  ASSERT_TRUE(child.ok());
  auto mutable_child_group = child.ValueOrDie()->AcquireMutable();
  EXPECT_OK(mutable_child_group->AddAttribute("val1", TestAttr<int32>()));
}

TEST_F(AttributeGroupTest, CanAccessIntegerAttributes) {
  auto mutable_group = group_->AcquireMutable();
  ASSERT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
  ASSERT_OK(mutable_group->AddAttribute("int64_val", TestAttr<int64>()));
  ASSERT_OK(mutable_group->AddAttribute("uint32_val", TestAttr<uint32>()));
  ASSERT_OK(mutable_group->AddAttribute("uint64_val", TestAttr<uint64>()));
  EXPECT_THAT(mutable_group->GetAttribute("int32_val"),
              IsOkAndContainsValue<int32>(kInt32TestVal));
  EXPECT_THAT(mutable_group->GetAttribute("int64_val"),
              IsOkAndContainsValue<int64>(kInt64TestVal));
  EXPECT_THAT(mutable_group->GetAttribute("uint32_val"),
              IsOkAndContainsValue<uint32>(kUInt32TestVal));
  EXPECT_THAT(mutable_group->GetAttribute("uint64_val"),
              IsOkAndContainsValue<uint64>(kUInt64TestVal));
}

TEST_F(AttributeGroupTest, CanAccessOtherAttributes) {
  auto mutable_group = group_->AcquireMutable();
  ASSERT_OK(mutable_group->AddAttribute("float_val", TestAttr<float>()));
  ASSERT_OK(mutable_group->AddAttribute("double_val", TestAttr<double>()));
  ASSERT_OK(mutable_group->AddAttribute("bool_val", TestAttr<bool>()));
  ASSERT_OK(mutable_group->AddAttribute("string_val", TestAttr<std::string>()));
  EXPECT_THAT(mutable_group->GetAttribute("float_val"),
              IsOkAndContainsValue<float>(kFloatTestVal));
  EXPECT_THAT(mutable_group->GetAttribute("double_val"),
              IsOkAndContainsValue<double>(kDoubleTestVal));
  EXPECT_THAT(mutable_group->GetAttribute("bool_val"),
              IsOkAndContainsValue<bool>(kBoolTestVal));
  EXPECT_THAT(mutable_group->GetAttribute("string_val"),
              IsOkAndContainsValue<std::string>(kStringTestVal));
}

TEST_F(AttributeGroupTest, CanAccessEnumAttributes) {
  auto mutable_group = group_->AcquireMutable();
  ASSERT_OK(mutable_group->AddAttribute(
      "top_val", TestAttr<const google::protobuf::EnumValueDescriptor*>()));
  EXPECT_THAT(mutable_group->GetAttribute("top_val"),
              IsOkAndContainsValue(kEnumTestVal));
}

TEST_F(AttributeGroupTest, CanReadAttribute) {
  auto mutable_group = group_->AcquireMutable();
  ASSERT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
  auto int32_val = mutable_group->ReadAttribute<int32>("int32_val");
  ASSERT_TRUE(int32_val.ok());
  EXPECT_EQ(int32_val.ValueOrDie(), kInt32TestVal);
}

TEST_F(AttributeGroupTest, CannotReadMissingAttribute) {
  auto readable_group = group_->AcquireReadable();
  EXPECT_FALSE(readable_group->ReadAttribute<int32>("int32_val").ok());
}

TEST_F(AttributeGroupTest, CannotReadAttributeWithWrongType) {
  auto mutable_group = group_->AcquireMutable();
  ASSERT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
  EXPECT_FALSE(mutable_group->ReadAttribute<int64>("int32_val").ok());
}

TEST_F(AttributeGroupTest, CanAccessChildGroups) {
  auto mutable_group = group_->AcquireMutable();
  ASSERT_TRUE(mutable_group->AddChildGroup("single_sub").ok());
  ASSERT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  ASSERT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  EXPECT_TRUE(mutable_group->GetChildGroup("single_sub").ok());
  EXPECT_TRUE(mutable_group->GetRepeatedChildGroup("repeated_sub", 0).ok());
  EXPECT_TRUE(mutable_group->GetRepeatedChildGroup("repeated_sub", 1).ok());
}

TEST_F(AttributeGroupTest, CanOnlyAccessRepeatedRange) {
  auto mutable_group = group_->AcquireMutable();
  ASSERT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  ASSERT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  EXPECT_FALSE(mutable_group->GetRepeatedChildGroup("repeated_sub", -1).ok());
  EXPECT_TRUE(mutable_group->GetRepeatedChildGroup("repeated_sub", 0).ok());
  EXPECT_TRUE(mutable_group->GetRepeatedChildGroup("repeated_sub", 1).ok());
  EXPECT_FALSE(mutable_group->GetRepeatedChildGroup("repeated_sub", 2).ok());
}

TEST_F(AttributeGroupTest, CanAccessAttributeFromChildGroup) {
  auto mutable_group = group_->AcquireMutable();
  auto child = mutable_group->AddChildGroup("single_sub");
  ASSERT_TRUE(child.ok());
  {
    auto mutable_child_group = child.ValueOrDie()->AcquireMutable();
    ASSERT_OK(mutable_child_group->AddAttribute("val1", TestAttr<int32>()));
  }
  auto found_child = mutable_group->GetChildGroup("single_sub");
  ASSERT_TRUE(found_child.ok());
  auto mutable_child_group = child.ValueOrDie()->AcquireMutable();
  EXPECT_THAT(mutable_child_group->GetAttribute("val1"),
              IsOkAndContainsValue<int32>(kInt32TestVal));
}

TEST_F(AttributeGroupTest, CanAccessAttrsInRepeatedChildGroups) {
  auto mutable_group = group_->AcquireMutable();
  ::util::StatusOr<AttributeGroup*> child;
  child = mutable_group->AddRepeatedChildGroup("repeated_sub");
  ASSERT_TRUE(child.ok());
  {
    auto mutable_child_group = child.ValueOrDie()->AcquireMutable();
    ASSERT_OK(mutable_child_group->AddAttribute("val1", TestAttr<int32>()));
  }
  child = mutable_group->AddRepeatedChildGroup("repeated_sub");
  ASSERT_TRUE(child.ok());
  {
    auto mutable_child_group = child.ValueOrDie()->AcquireMutable();
    ASSERT_OK(
        mutable_child_group->AddAttribute("val2", TestAttr<std::string>()));
  }
  {
    auto found_child = mutable_group->GetRepeatedChildGroup("repeated_sub", 0);
    ASSERT_TRUE(found_child.ok());
    auto readable_child_group = found_child.ValueOrDie()->AcquireReadable();
    EXPECT_THAT(readable_child_group->GetAttribute("val1"),
              IsOkAndContainsValue<int32>(kInt32TestVal));
    EXPECT_FALSE(readable_child_group->GetAttribute("val2").ok());
  }
  {
    auto found_child = mutable_group->GetRepeatedChildGroup("repeated_sub", 1);
    ASSERT_TRUE(found_child.ok());
    auto readable_child_group = found_child.ValueOrDie()->AcquireReadable();
    EXPECT_THAT(readable_child_group->GetAttribute("val2"),
                IsOkAndContainsValue<std::string>(kStringTestVal));
    EXPECT_FALSE(readable_child_group->GetAttribute("val1").ok());
  }
}

TEST_F(AttributeGroupTest, CannotAccessFakeFields) {
  auto readable_group = group_->AcquireReadable();
  EXPECT_FALSE(readable_group->GetAttribute("not_a_val").ok());
  EXPECT_FALSE(readable_group->GetChildGroup("not_a_group").ok());
  EXPECT_FALSE(
      readable_group->GetRepeatedChildGroup("not_a_repeated_group", 0).ok());
}

TEST_F(AttributeGroupTest, CannotAccessUnsetFields) {
  auto readable_group = group_->AcquireReadable();
  EXPECT_FALSE(readable_group->GetAttribute("int32_val").ok());
  EXPECT_FALSE(readable_group->GetChildGroup("single_sub").ok());
  EXPECT_FALSE(readable_group->GetRepeatedChildGroup("repeated_sub", 0).ok());
}

TEST_F(AttributeGroupTest, CanCheckHasAttribute) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_FALSE(mutable_group->HasAttribute("int32_val"));
  EXPECT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
  EXPECT_TRUE(mutable_group->HasAttribute("int32_val"));
}

TEST_F(AttributeGroupTest, CanCheckHasChildGroup) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_FALSE(mutable_group->HasChildGroup("single_sub"));
  ASSERT_TRUE(mutable_group->AddChildGroup("single_sub").ok());
  EXPECT_TRUE(mutable_group->HasChildGroup("single_sub"));
}

TEST_F(AttributeGroupTest, CanGetAttributeNames) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_THAT(mutable_group->GetAttributeNames(), testing::IsEmpty());
  ASSERT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
  ASSERT_OK(mutable_group->AddAttribute("int64_val", TestAttr<int64>()));
  EXPECT_THAT(mutable_group->GetAttributeNames(),
              testing::UnorderedElementsAre("int32_val", "int64_val"));
}

TEST_F(AttributeGroupTest, CanGetChildGroupNames) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_THAT(mutable_group->GetChildGroupNames(), testing::IsEmpty());
  ASSERT_TRUE(mutable_group->AddChildGroup("single_sub").ok());
  EXPECT_THAT(mutable_group->GetChildGroupNames(),
              testing::ElementsAre("single_sub"));
  ASSERT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  EXPECT_THAT(mutable_group->GetChildGroupNames(),
              testing::ElementsAre("single_sub"));
}

TEST_F(AttributeGroupTest, CanGetRepeatedChildGroupNames) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_THAT(mutable_group->GetRepeatedChildGroupNames(), testing::IsEmpty());
  ASSERT_TRUE(mutable_group->AddChildGroup("single_sub").ok());
  EXPECT_THAT(mutable_group->GetRepeatedChildGroupNames(), testing::IsEmpty());
  ASSERT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  EXPECT_THAT(mutable_group->GetRepeatedChildGroupNames(),
              testing::ElementsAre("repeated_sub"));
  ASSERT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  EXPECT_THAT(mutable_group->GetRepeatedChildGroupNames(),
              testing::ElementsAre("repeated_sub"));
}

TEST_F(AttributeGroupTest, CanGetRepeatedChildGroupSize) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_TRUE(mutable_group->GetRepeatedChildGroupSize("repeated_sub").ok());
  EXPECT_EQ(
      mutable_group->GetRepeatedChildGroupSize("repeated_sub").ValueOrDie(), 0);
  ASSERT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  EXPECT_TRUE(mutable_group->GetRepeatedChildGroupSize("repeated_sub").ok());
  EXPECT_EQ(
      mutable_group->GetRepeatedChildGroupSize("repeated_sub").ValueOrDie(), 1);
  ASSERT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  EXPECT_EQ(
      mutable_group->GetRepeatedChildGroupSize("repeated_sub").ValueOrDie(), 2);
}

TEST_F(AttributeGroupTest, VersionIdChangesOnModification) {
  auto mutable_group = group_->AcquireMutable();
  AttributeGroupVersionId version_id = mutable_group->GetVersionId();
  // Add an attribute.
  EXPECT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
  EXPECT_FALSE(mutable_group->GetVersionId() == version_id);
  version_id = mutable_group->GetVersionId();
  // Add a child group.
  EXPECT_TRUE(mutable_group->AddChildGroup("single_sub").ok());
  EXPECT_FALSE(mutable_group->GetVersionId() == version_id);
  version_id = mutable_group->GetVersionId();
  // Add a repeated child group.
  EXPECT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  EXPECT_FALSE(mutable_group->GetVersionId() == version_id);
}

TEST_F(AttributeGroupTest, VersionIdDoesNotChangeWithoutModification) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
  EXPECT_TRUE(mutable_group->AddChildGroup("single_sub").ok());
  EXPECT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  AttributeGroupVersionId version_id = mutable_group->GetVersionId();
  // Get an attribute.
  EXPECT_TRUE(mutable_group->GetAttribute("int32_val").ok());
  EXPECT_EQ(mutable_group->GetVersionId(), version_id);
  // Get a child group.
  EXPECT_TRUE(mutable_group->GetChildGroup("single_sub").ok());
  EXPECT_EQ(mutable_group->GetVersionId(), version_id);
  // Get a repeated child group.
  EXPECT_TRUE(mutable_group->GetRepeatedChildGroup("repeated_sub", 0).ok());
  EXPECT_EQ(mutable_group->GetVersionId(), version_id);
}

TEST_F(AttributeGroupTest, CanRemoveAttribute) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_THAT(mutable_group->GetAttributeNames(), testing::IsEmpty());
  EXPECT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
  EXPECT_THAT(mutable_group->GetAttributeNames(),
              testing::ElementsAre("int32_val"));
  EXPECT_OK(mutable_group->RemoveAttribute("int32_val"));
  EXPECT_THAT(mutable_group->GetAttributeNames(), testing::IsEmpty());
}

TEST_F(AttributeGroupTest, CannotRemoveInvalidAttribute) {
  auto mutable_group = group_->AcquireMutable();
  // Can remove attribute that exists but hasn't been added.
  EXPECT_OK(mutable_group->RemoveAttribute("int32_val"));
  // But can't remove attribute group.
  EXPECT_TRUE(mutable_group->AddChildGroup("single_sub").ok());
  EXPECT_FALSE(mutable_group->RemoveAttribute("single_sub").ok());
  // Can't remove repeated attribute group.
  EXPECT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  EXPECT_FALSE(mutable_group->RemoveAttribute("repeated_sub").ok());
  // Can't remove attribute that isn't in schema.
  EXPECT_FALSE(mutable_group->RemoveAttribute("doesnt_exist").ok());
}

TEST_F(AttributeGroupTest, CanAddAttributeAfterRemoving) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
  EXPECT_OK(mutable_group->RemoveAttribute("int32_val"));
  EXPECT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
}

TEST_F(AttributeGroupTest, CanRemoveChildGroup) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_THAT(mutable_group->GetChildGroupNames(), testing::IsEmpty());
  EXPECT_TRUE(mutable_group->AddChildGroup("single_sub").ok());
  EXPECT_THAT(mutable_group->GetChildGroupNames(),
              testing::ElementsAre("single_sub"));
  EXPECT_OK(mutable_group->RemoveChildGroup("single_sub"));
  EXPECT_THAT(mutable_group->GetChildGroupNames(), testing::IsEmpty());
}

TEST_F(AttributeGroupTest, CannotRemoveInvalidChildGroup) {
  auto mutable_group = group_->AcquireMutable();
  // Can remove attribute group that hasn't been added.
  EXPECT_OK(mutable_group->RemoveChildGroup("single_sub"));
  // But can't remove repeated attribute group.
  EXPECT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  EXPECT_FALSE(mutable_group->RemoveChildGroup("repeated_sub").ok());
  // Can't remove attribute as attribute group.
  EXPECT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
  EXPECT_FALSE(mutable_group->RemoveChildGroup("int32_val").ok());
  // Can't remove attribute group that isn't in schema.
  EXPECT_FALSE(mutable_group->RemoveChildGroup("doesnt_exist").ok());
}

TEST_F(AttributeGroupTest, CanRemoveRepeatedChildGroup) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_THAT(mutable_group->GetRepeatedChildGroupNames(), testing::IsEmpty());
  EXPECT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  EXPECT_THAT(mutable_group->GetRepeatedChildGroupNames(),
              testing::ElementsAre("repeated_sub"));
  EXPECT_OK(mutable_group->RemoveRepeatedChildGroup("repeated_sub"));
  EXPECT_THAT(mutable_group->GetRepeatedChildGroupNames(), testing::IsEmpty());
}

TEST_F(AttributeGroupTest, CanRemoveMultipleRepeatedChildGroups) {
  auto mutable_group = group_->AcquireMutable();
  EXPECT_THAT(mutable_group->GetRepeatedChildGroupNames(), testing::IsEmpty());
  EXPECT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  EXPECT_TRUE(mutable_group->AddRepeatedChildGroup("repeated_sub").ok());
  EXPECT_THAT(mutable_group->GetRepeatedChildGroupNames(),
              testing::ElementsAre("repeated_sub"));
  EXPECT_EQ(
      mutable_group->GetRepeatedChildGroupSize("repeated_sub").ValueOrDie(), 2);
  EXPECT_OK(mutable_group->RemoveRepeatedChildGroup("repeated_sub"));
  EXPECT_THAT(mutable_group->GetRepeatedChildGroupNames(), testing::IsEmpty());
  EXPECT_EQ(
      mutable_group->GetRepeatedChildGroupSize("repeated_sub").ValueOrDie(), 0);
}

TEST_F(AttributeGroupTest, CannotRemoveInvalidRepeatedChildGroup) {
  auto mutable_group = group_->AcquireMutable();
  // Can remove empty repeated child group.
  EXPECT_OK(mutable_group->RemoveRepeatedChildGroup("repeated_sub"));
  // But can't remove singular attribute group as repeated.
  EXPECT_TRUE(mutable_group->AddChildGroup("single_sub").ok());
  EXPECT_FALSE(mutable_group->RemoveRepeatedChildGroup("single_sub").ok());
  // Can't remove attribute as repeated group.
  EXPECT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
  EXPECT_FALSE(mutable_group->RemoveRepeatedChildGroup("int32_val").ok());
  // Can't remove repeated attribute group that isn't in schema.
  EXPECT_FALSE(mutable_group->RemoveRepeatedChildGroup("doesnt_exist").ok());
}

class AttributeGroupQueryTest : public ::testing::Test {
 public:
  AttributeGroupQueryTest() {
    group_ = AttributeGroup::From(TestTop::descriptor());
  }

 protected:
  template <typename T>
  ManagedAttribute* TestAttr() {
    // We need to keep every datasource in scope, but by sticking them in a
    // vector we can avoid cluttering our tests with lots of datasources.
    auto fixed_datasource = FixedDataSource<T>::Make(GetTestVal<T>());
    managed_datasources_.push_back(fixed_datasource);
    return fixed_datasource->GetAttribute();
  }

  ::util::Status AddSingleQueryPath() {
    auto mutable_group = group_->AcquireMutable();
    ASSIGN_OR_RETURN(auto single_sub,
                     mutable_group->AddChildGroup("single_sub"));
    auto mutable_single_sub = single_sub->AcquireMutable();
    return mutable_single_sub->AddAttribute("val1", TestAttr<int32>());
  }

  ::util::Status RemoveSingleQueryPath() {
    auto mutable_group = group_->AcquireMutable();
    return mutable_group->RemoveChildGroup("single_sub");
  }

  ::util::Status AddRepeatedQueryPath() {
    auto mutable_group = group_->AcquireMutable();
    ASSIGN_OR_RETURN(auto repeated_sub,
                     mutable_group->AddRepeatedChildGroup("repeated_sub"));
    auto mutable_repeated_sub = repeated_sub->AcquireMutable();
    return mutable_repeated_sub->AddAttribute("val1", TestAttr<int32>());
  }

  ::util::Status RemoveRepeatedQueryPaths() {
    auto mutable_group = group_->AcquireMutable();
    return mutable_group->RemoveRepeatedChildGroup("repeated_sub");
  }

  std::unique_ptr<AttributeGroup> group_;
  std::vector<std::shared_ptr<DataSource>> managed_datasources_;
};

TEST_F(AttributeGroupQueryTest, CanRegisterAndUnregisterNewQuery) {
  ASSERT_OK(AddSingleQueryPath());
  DummyThreadpool threadpool;
  AttributeGroupQuery query(group_.get(), &threadpool);
  EXPECT_OK(group_->AcquireReadable()->RegisterQuery(
      &query,
      {{PathEntry("single_sub"), PathEntry("val1")}}));
  group_->AcquireReadable()->UnregisterQuery(&query);
}

TEST_F(AttributeGroupQueryTest, CanRegisterQueryWithMissingFields) {
  DummyThreadpool threadpool;
  AttributeGroupQuery query(group_.get(), &threadpool);
  EXPECT_OK(group_->AcquireReadable()->RegisterQuery(
      &query, {{PathEntry("repeated_sub", 2000), PathEntry("val1")}}));
}

TEST_F(AttributeGroupQueryTest, CannotRegisterMalformedQueries) {
  DummyThreadpool threadpool;
  AttributeGroupQuery query(group_.get(), &threadpool);
  EXPECT_THAT(
      group_->AcquireReadable()->RegisterQuery(
          &query, {{PathEntry("single_sub"), PathEntry("doesn't exist")}}),
      StatusIs(_, _, HasSubstr("No such field \"doesn't exist\"")));
  EXPECT_THAT(
      group_->AcquireReadable()->RegisterQuery(
          &query, {{PathEntry("single_sub", 0), PathEntry("val1")}}),
      StatusIs(_, _,
               HasSubstr("\"single_sub\" is a singular attribute group")));
  EXPECT_THAT(
      group_->AcquireReadable()->RegisterQuery(
          &query, {{PathEntry("repeated_sub"), PathEntry("val1")}}),
      StatusIs(_, _,
               HasSubstr("\"repeated_sub\" is a repeated attribute group")));
  EXPECT_THAT(group_->AcquireReadable()->RegisterQuery(
                  &query, {{PathEntry("single_sub")}}),
              StatusIs(_, _, HasSubstr("not marked as a terminal group")));
  EXPECT_THAT(group_->AcquireReadable()->RegisterQuery(
                  &query, {{PathEntry("repeated_sub", -1), PathEntry("val1")}}),
              StatusIs(_, _, HasSubstr("negative index")));
  EXPECT_THAT(
      group_->AcquireReadable()->RegisterQuery(
          &query, {{PathEntry("single_sub"), PathEntry("val1"),
                    PathEntry("val1_sub")}}),
      StatusIs(
          _, _,
          HasSubstr(
              "attribute \"val1\" somewhere other than the last position")));
  PathEntry terminal_attribute("val1");
  terminal_attribute.terminal_group = true;
  EXPECT_THAT(
      group_->AcquireReadable()->RegisterQuery(
          &query, {{PathEntry("single_sub"), terminal_attribute}}),
      StatusIs(_, _,
               HasSubstr("marks the attribute \"val1\" as a terminal group")));
}

TEST_F(AttributeGroupQueryTest, CanTraverseQuery) {
  ASSERT_OK(AddSingleQueryPath());
  DummyThreadpool threadpool;
  AttributeGroupQuery query(group_.get(), &threadpool);
  ASSERT_OK(group_->AcquireReadable()->RegisterQuery(
      &query,
      {{PathEntry("single_sub"), PathEntry("val1")}}));
  MockFunction<::util::Status(std::unique_ptr<ReadableAttributeGroup>)>
      group_function;
  MockFunction<::util::Status(ManagedAttribute*, const Path& querying_path,
                              const AttributeSetterFunction& setter)>
      attribute_function;

  // We traverse three groups to query the single attribute "val1".
  EXPECT_CALL(group_function, Call(_))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(attribute_function, Call(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(group_->TraverseQuery(&query, group_function.AsStdFunction(),
                                  attribute_function.AsStdFunction()));
}

TEST_F(AttributeGroupQueryTest, CanTraverseQueryAfterModification) {
  DummyThreadpool threadpool;
  AttributeGroupQuery query(group_.get(), &threadpool);
  ASSERT_OK(group_->AcquireReadable()->RegisterQuery(
      &query,
      {{PathEntry("single_sub"), PathEntry("val1")}}));
  MockFunction<::util::Status(std::unique_ptr<ReadableAttributeGroup>)>
      group_function;
  MockFunction<::util::Status(ManagedAttribute*, const Path& querying_path,
                              const AttributeSetterFunction& setter)>
      attribute_function;

  // We traverse one group, since single_sub is missing.
  EXPECT_CALL(group_function, Call(_)).WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(group_->TraverseQuery(&query, group_function.AsStdFunction(),
                                  attribute_function.AsStdFunction()));

  ASSERT_OK(AddSingleQueryPath());

  // We traverse two groups to query the single attribute "val1".
  EXPECT_CALL(group_function, Call(_))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(attribute_function, Call(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(group_->TraverseQuery(&query, group_function.AsStdFunction(),
                                  attribute_function.AsStdFunction()));

  ASSERT_OK(RemoveSingleQueryPath());

  // We're back to our original behavior.
  EXPECT_CALL(group_function, Call(_)).WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(group_->TraverseQuery(&query, group_function.AsStdFunction(),
                                  attribute_function.AsStdFunction()));
}

TEST_F(AttributeGroupQueryTest, CanCallQueryGet) {
  ASSERT_OK(AddSingleQueryPath());
  ASSERT_OK(AddRepeatedQueryPath());
  DummyThreadpool threadpool;
  AttributeGroupQuery query(group_.get(), &threadpool);
  ASSERT_OK(group_->AcquireReadable()->RegisterQuery(
      &query, {{PathEntry("single_sub"), PathEntry("val1")},
               {PathEntry("repeated_sub", 0), PathEntry("val1")}}));

  TestTop result;
  ASSERT_OK(query.Get(&result));
  ASSERT_TRUE(result.has_single_sub());
  EXPECT_EQ(result.single_sub().val1(), kInt32TestVal);
  ASSERT_EQ(result.repeated_sub_size(), 1);
  EXPECT_EQ(result.repeated_sub(0).val1(), kInt32TestVal);
}

TEST_F(AttributeGroupQueryTest, CanGetAllFieldTypes) {
  {
    // Add one attribute of every available type.
    auto mutable_group = group_->AcquireMutable();
    ASSERT_OK(mutable_group->AddAttribute("int32_val", TestAttr<int32>()));
    ASSERT_OK(mutable_group->AddAttribute("int64_val", TestAttr<int64>()));
    ASSERT_OK(mutable_group->AddAttribute("uint32_val", TestAttr<uint32>()));
    ASSERT_OK(mutable_group->AddAttribute("uint64_val", TestAttr<uint64>()));
    ASSERT_OK(mutable_group->AddAttribute("float_val", TestAttr<float>()));
    ASSERT_OK(mutable_group->AddAttribute("double_val", TestAttr<double>()));
    ASSERT_OK(mutable_group->AddAttribute("bool_val", TestAttr<bool>()));
    ASSERT_OK(
        mutable_group->AddAttribute("string_val", TestAttr<std::string>()));
    ASSERT_OK(mutable_group->AddAttribute(
        "top_val", TestAttr<const google::protobuf::EnumValueDescriptor*>()));
  }
  DummyThreadpool threadpool;
  AttributeGroupQuery query(group_.get(), &threadpool);
  ASSERT_OK(group_->AcquireReadable()->RegisterQuery(&query,
                                                     {{PathEntry("int32_val")},
                                                      {PathEntry("int64_val")},
                                                      {PathEntry("uint32_val")},
                                                      {PathEntry("uint64_val")},
                                                      {PathEntry("float_val")},
                                                      {PathEntry("double_val")},
                                                      {PathEntry("bool_val")},
                                                      {PathEntry("string_val")},
                                                      {PathEntry("top_val")}}));
  TestTop result;
  ASSERT_OK(query.Get(&result));
  EXPECT_EQ(result.int32_val(), kInt32TestVal);
  EXPECT_EQ(result.int64_val(), kInt64TestVal);
  EXPECT_EQ(result.uint32_val(), kUInt32TestVal);
  EXPECT_EQ(result.uint64_val(), kUInt64TestVal);
  EXPECT_EQ(result.float_val(), kFloatTestVal);
  EXPECT_EQ(result.double_val(), kDoubleTestVal);
  EXPECT_EQ(result.bool_val(), kBoolTestVal);
  EXPECT_EQ(result.string_val(), kStringTestVal);
  EXPECT_EQ(result.top_val(), TopEnum::TWO);
}

TEST_F(AttributeGroupQueryTest, CanCallQueryGetAfterModification) {
  DummyThreadpool threadpool;
  AttributeGroupQuery query(group_.get(), &threadpool);
  ASSERT_OK(group_->AcquireReadable()->RegisterQuery(
      &query,
      {{PathEntry("single_sub"), PathEntry("val1")}}));

  TestTop result;

  ASSERT_OK(query.Get(&result));
  EXPECT_FALSE(result.has_single_sub());

  ASSERT_OK(AddSingleQueryPath());

  ASSERT_OK(query.Get(&result));
  ASSERT_TRUE(result.has_single_sub());
  EXPECT_EQ(result.single_sub().val1(), kInt32TestVal);

  {
    auto mutable_group = group_->AcquireMutable();
    ASSERT_OK_AND_ASSIGN(auto single_sub,
                         mutable_group->GetChildGroup("single_sub"));
    ASSERT_OK(single_sub->AcquireMutable()->RemoveAttribute("val1"));
  }

  ASSERT_OK(query.Get(&result));
  ASSERT_TRUE(result.has_single_sub());
  EXPECT_EQ(result.single_sub().val1(), 0);

  ASSERT_OK(RemoveSingleQueryPath());

  ASSERT_OK(query.Get(&result));
  EXPECT_FALSE(result.has_single_sub());
}

TEST_F(AttributeGroupQueryTest, MultipleQueryPathsUpdateSeparately) {
  DummyThreadpool threadpool;
  AttributeGroupQuery query(group_.get(), &threadpool);
  ASSERT_OK(group_->AcquireReadable()->RegisterQuery(
      &query, {{PathEntry("single_sub"), PathEntry("val1")},
               {PathEntry("repeated_sub", 0), PathEntry("val1")}}));

  TestTop result;

  ASSERT_OK(query.Get(&result));
  EXPECT_FALSE(result.has_single_sub());
  EXPECT_EQ(result.repeated_sub_size(), 0);

  ASSERT_OK(AddSingleQueryPath());

  ASSERT_OK(query.Get(&result));
  ASSERT_TRUE(result.has_single_sub());
  EXPECT_EQ(result.single_sub().val1(), kInt32TestVal);
  EXPECT_EQ(result.repeated_sub_size(), 0);

  ASSERT_OK(AddRepeatedQueryPath());
  ASSERT_OK(RemoveSingleQueryPath());

  ASSERT_OK(query.Get(&result));
  ASSERT_FALSE(result.has_single_sub());
  ASSERT_EQ(result.repeated_sub_size(), 1);
  EXPECT_EQ(result.repeated_sub(0).val1(), kInt32TestVal);

  ASSERT_OK(RemoveRepeatedQueryPaths());

  ASSERT_OK(query.Get(&result));
  EXPECT_FALSE(result.has_single_sub());
  EXPECT_EQ(result.repeated_sub_size(), 0);
}

TEST_F(AttributeGroupQueryTest, QueryUsesCorrectRepeatedPath) {
  DummyThreadpool threadpool;
  AttributeGroupQuery query(group_.get(), &threadpool);
  ASSERT_OK(group_->AcquireReadable()->RegisterQuery(
      &query, {{PathEntry("repeated_sub", 1), PathEntry("val1")}}));

  TestTop result;

  ASSERT_OK(AddRepeatedQueryPath());

  // There is a repeated_sub, but it's in position 0 rather than 1 so our query
  // doesn't include it.
  ASSERT_OK(query.Get(&result));
  EXPECT_EQ(result.repeated_sub_size(), 0);

  ASSERT_OK(AddRepeatedQueryPath());

  ASSERT_OK(query.Get(&result));
  ASSERT_EQ(result.repeated_sub_size(), 2);
  EXPECT_EQ(result.repeated_sub(0).val1(), 0);
  EXPECT_EQ(result.repeated_sub(1).val1(), kInt32TestVal);

  ASSERT_OK(RemoveRepeatedQueryPaths());

  ASSERT_OK(query.Get(&result));
  EXPECT_EQ(result.repeated_sub_size(), 0);
}

TEST_F(AttributeGroupQueryTest, CanQueryAllRepeatedFields) {
  DummyThreadpool threadpool;
  AttributeGroupQuery query(group_.get(), &threadpool);
  PathEntry repeated_entry("repeated_sub");
  repeated_entry.indexed = true;
  repeated_entry.all = true;
  ASSERT_OK(group_->AcquireReadable()->RegisterQuery(
      &query, {{repeated_entry, PathEntry("val1")}}));

  TestTop result;

  ASSERT_OK(query.Get(&result));
  EXPECT_EQ(result.repeated_sub_size(), 0);

  ASSERT_OK(AddRepeatedQueryPath());

  ASSERT_OK(query.Get(&result));
  ASSERT_EQ(result.repeated_sub_size(), 1);
  EXPECT_EQ(result.repeated_sub(0).val1(), kInt32TestVal);

  ASSERT_OK(AddRepeatedQueryPath());

  ASSERT_OK(query.Get(&result));
  ASSERT_EQ(result.repeated_sub_size(), 2);
  EXPECT_EQ(result.repeated_sub(0).val1(), kInt32TestVal);
  EXPECT_EQ(result.repeated_sub(1).val1(), kInt32TestVal);
}

TEST_F(AttributeGroupQueryTest, CanQueryTerminalGroup) {
  DummyThreadpool threadpool;
  AttributeGroupQuery query(group_.get(), &threadpool);
  PathEntry terminal_group("single_sub");
  terminal_group.terminal_group = true;
  ASSERT_OK(
      group_->AcquireReadable()->RegisterQuery(&query, {{terminal_group}}));

  TestTop result;

  ASSERT_OK(query.Get(&result));
  EXPECT_FALSE(result.has_single_sub());

  ASSERT_OK(AddSingleQueryPath());

  ASSERT_OK(query.Get(&result));
  ASSERT_TRUE(result.has_single_sub());
  EXPECT_EQ(result.single_sub().val1(), kInt32TestVal);
}

TEST_F(AttributeGroupQueryTest, CanQueryRepeatedTerminalGroup) {
  DummyThreadpool threadpool;
  AttributeGroupQuery query(group_.get(), &threadpool);
  PathEntry terminal_group("repeated_sub", 1);
  terminal_group.terminal_group = true;
  ASSERT_OK(
      group_->AcquireReadable()->RegisterQuery(&query, {{terminal_group}}));

  TestTop result;

  ASSERT_OK(query.Get(&result));
  EXPECT_EQ(result.repeated_sub_size(), 0);

  ASSERT_OK(AddRepeatedQueryPath());

  ASSERT_OK(query.Get(&result));
  EXPECT_EQ(result.repeated_sub_size(), 0);

  ASSERT_OK(AddRepeatedQueryPath());

  ASSERT_OK(query.Get(&result));
  ASSERT_EQ(result.repeated_sub_size(), 2);
  EXPECT_EQ(result.repeated_sub(1).val1(), kInt32TestVal);
}

class AttributeGroupSetTest : public ::testing::Test {
 public:
  AttributeGroupSetTest() {
    group_ = AttributeGroup::From(TestTop::descriptor());
  }

 protected:
  std::unique_ptr<AttributeGroup> group_;
};

TEST_F(AttributeGroupSetTest, CanSetSingleAttribute) {
  DataSourceMock datasource;
  ManagedAttributeMock attribute;

  EXPECT_CALL(attribute, GetValue()).WillOnce(Return(0));
  EXPECT_CALL(attribute, CanSet()).WillOnce(Return(true));
  EXPECT_CALL(attribute, Set(Attribute(1234)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(attribute, GetDataSource())
      .Times(2)
      .WillRepeatedly(Return(&datasource));
  EXPECT_CALL(datasource, LockAndFlushWrites())
      .WillOnce(Return(::util::OkStatus()));

  Path path{PathEntry("int32_val")};
  DummyThreadpool threadpool;
  ASSERT_OK(group_->AcquireMutable()->AddAttribute("int32_val", &attribute));
  EXPECT_OK(group_->Set({{path, 1234}}, &threadpool));
}

TEST_F(AttributeGroupSetTest, CanSetMultipleAttributesWithSameDataSource) {
  DataSourceMock datasource;
  ManagedAttributeMock attribute1;
  ManagedAttributeMock attribute2;

  EXPECT_CALL(attribute1, GetValue()).WillOnce(Return(0));
  EXPECT_CALL(attribute1, CanSet()).WillOnce(Return(true));
  EXPECT_CALL(attribute1, Set(Attribute(1234)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(attribute1, GetDataSource())
      .Times(2)
      .WillRepeatedly(Return(&datasource));
  EXPECT_CALL(attribute2, GetValue()).WillOnce(Return(int64{0}));
  EXPECT_CALL(attribute2, CanSet()).WillOnce(Return(true));
  EXPECT_CALL(attribute2, Set(Attribute(5678)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(attribute2, GetDataSource())
      .Times(2)
      .WillRepeatedly(Return(&datasource));
  // LockAndFlushWrites is only called once, since both attributes have the same
  // datasource.
  EXPECT_CALL(datasource, LockAndFlushWrites())
      .WillOnce(Return(::util::OkStatus()));

  Path path1{PathEntry("int32_val")};
  Path path2{PathEntry("int64_val")};
  DummyThreadpool threadpool;
  ASSERT_OK(group_->AcquireMutable()->AddAttribute("int32_val", &attribute1));
  ASSERT_OK(group_->AcquireMutable()->AddAttribute("int64_val", &attribute2));
  EXPECT_OK(group_->Set({{path1, 1234}, {path2, 5678}}, &threadpool));
}

TEST_F(AttributeGroupSetTest, CannotSetUnsettableAttribute) {
  DataSourceMock datasource;
  ManagedAttributeMock attribute;

  EXPECT_CALL(attribute, GetValue()).WillOnce(Return(0));
  EXPECT_CALL(attribute, CanSet()).WillOnce(Return(false));
  EXPECT_CALL(attribute, GetDataSource()).WillOnce(Return(&datasource));

  Path path{PathEntry("int32_val")};
  DummyThreadpool threadpool;
  ASSERT_OK(group_->AcquireMutable()->AddAttribute("int32_val", &attribute));
  // TODO(swiggett): Replace with a status matcher.
  EXPECT_FALSE(group_->Set({{path, 1234}}, &threadpool).ok());
}

}  // namespace
}  // namespace phal
}  // namespace hal
}  // namespace stratum
