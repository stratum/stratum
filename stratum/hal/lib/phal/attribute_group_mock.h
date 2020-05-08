// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_PHAL_ATTRIBUTE_GROUP_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_ATTRIBUTE_GROUP_MOCK_H_

#include "stratum/hal/lib/phal/attribute_group.h"

#include <memory>
#include <utility>
#include <string>
#include <vector>
#include <set>

#include "gmock/gmock.h"
#include "absl/container/flat_hash_map.h"

namespace stratum {
namespace hal {
namespace phal {

class AttributeGroupMock;
std::unique_ptr<MutableAttributeGroup> MakeMockGroup(AttributeGroupMock* group);

class AttributeGroupMock : public AttributeGroup, public MutableAttributeGroup {
 public:
  explicit AttributeGroupMock(const google::protobuf::Descriptor* descriptor)
      : descriptor_(descriptor) {}

  std::unique_ptr<ReadableAttributeGroup> AcquireReadable() override {
    return MakeMockGroup(this);
  }
  std::unique_ptr<MutableAttributeGroup> AcquireMutable() override {
    return MakeMockGroup(this);
  }

  MOCK_METHOD3(
      TraverseQuery,
      ::util::Status(AttributeGroupQuery* query,
                     std::function<::util::Status(
                         std::unique_ptr<ReadableAttributeGroup> group)>
                         group_function,
                     std::function<::util::Status(
                         ManagedAttribute* attribute, const Path& querying_path,
                         const AttributeSetterFunction& setter)>
                         attribute_function));
  MOCK_METHOD2(Set, ::util::Status(const AttributeValueMap& values,
                                   ThreadpoolInterface* threadpool));

  MOCK_METHOD2(AddAttribute, ::util::Status(const std::string& name,
                                            ManagedAttribute* value));
  MOCK_METHOD1(AddChildGroup,
               ::util::StatusOr<AttributeGroup*>(const std::string& name));
  MOCK_METHOD1(AddRepeatedChildGroup,
               ::util::StatusOr<AttributeGroup*>(const std::string& name));
  MOCK_METHOD1(RemoveAttribute, ::util::Status(const std::string& name));
  MOCK_METHOD1(RemoveChildGroup, ::util::Status(const std::string& name));
  MOCK_METHOD1(RemoveRepeatedChildGroup,
               ::util::Status(const std::string& name));
  MOCK_METHOD1(
      AddRuntimeConfigurator,
      void(std::unique_ptr<AttributeGroup::RuntimeConfiguratorInterface>
               configurator));
  MOCK_CONST_METHOD1(GetAttribute, ::util::StatusOr<ManagedAttribute*>(
                                       const std::string& name));
  MOCK_CONST_METHOD1(GetChildGroup, ::util::StatusOr<AttributeGroup*>(
                                        const std::string& name));
  MOCK_CONST_METHOD2(GetRepeatedChildGroup,
                     ::util::StatusOr<AttributeGroup*>(const std::string& name,
                                                       int idx));
  MOCK_CONST_METHOD1(HasAttribute, bool(const std::string& name));
  MOCK_CONST_METHOD1(HasChildGroup, bool(const std::string& name));
  MOCK_CONST_METHOD0(GetAttributeNames, std::set<std::string>());
  MOCK_CONST_METHOD0(GetChildGroupNames, std::set<std::string>());
  MOCK_CONST_METHOD0(GetRepeatedChildGroupNames, std::set<std::string>());
  MOCK_CONST_METHOD1(GetRepeatedChildGroupSize,
                     ::util::StatusOr<int>(const std::string& name));
  MOCK_CONST_METHOD0(GetVersionId, AttributeGroupVersionId());

  MOCK_METHOD2(RegisterQuery, ::util::Status(AttributeGroupQuery* query,
                                             std::vector<Path> paths));
  MOCK_METHOD1(UnregisterQuery, void(AttributeGroupQuery* query));

  const google::protobuf::Descriptor* GetDescriptor() const override {
    return descriptor_;
  }

 private:
  const google::protobuf::Descriptor* descriptor_;
};

// This class just points all calls directly back to the
// AttributeGroupMock passed to the constructor. This makes setting
// up this mock easy, since we don't have to construct both a mock group and a
// mock readable/writable group.
class LockedAttributeGroupMock : public MutableAttributeGroup {
 public:
  explicit LockedAttributeGroupMock(AttributeGroupMock* group)
      : group_(group) {}
  ::util::Status AddAttribute(const std::string& name,
                              ManagedAttribute* value) override {
    return group_->AddAttribute(name, value);
  }
  ::util::StatusOr<AttributeGroup*> AddChildGroup(
      const std::string& name) override {
    return group_->AddChildGroup(name);
  }
  ::util::StatusOr<AttributeGroup*> AddRepeatedChildGroup(
      const std::string& name) override {
    return group_->AddRepeatedChildGroup(name);
  }
  ::util::Status RemoveAttribute(const std::string& name) override {
    return group_->RemoveAttribute(name);
  }
  ::util::Status RemoveChildGroup(const std::string& name) override {
    return group_->RemoveChildGroup(name);
  }
  ::util::Status RemoveRepeatedChildGroup(const std::string& name) override {
    return group_->RemoveRepeatedChildGroup(name);
  }
  void AddRuntimeConfigurator(
      std::unique_ptr<AttributeGroup::RuntimeConfiguratorInterface>
          configurator) override {
    return group_->AddRuntimeConfigurator(std::move(configurator));
  }
  // These must only be called if writer == true during construction.
  ::util::StatusOr<ManagedAttribute*> GetAttribute(
      const std::string& name) const override {
    return group_->GetAttribute(name);
  }
  ::util::StatusOr<AttributeGroup*> GetChildGroup(
      const std::string& name) const override {
    return group_->GetChildGroup(name);
  }
  ::util::StatusOr<AttributeGroup*> GetRepeatedChildGroup(
      const std::string& name, int idx) const override {
    return group_->GetRepeatedChildGroup(name, idx);
  }
  bool HasAttribute(const std::string& name) const override {
    return group_->HasAttribute(name);
  }
  bool HasChildGroup(const std::string& name) const override {
    return group_->HasChildGroup(name);
  }
  std::set<std::string> GetAttributeNames() const override {
    return group_->GetAttributeNames();
  }
  std::set<std::string> GetChildGroupNames() const override {
    return group_->GetChildGroupNames();
  }
  std::set<std::string> GetRepeatedChildGroupNames() const override {
    return group_->GetRepeatedChildGroupNames();
  }
  ::util::StatusOr<int> GetRepeatedChildGroupSize(
      const std::string& name) const override {
    return group_->GetRepeatedChildGroupSize(name);
  }
  const google::protobuf::Descriptor* GetDescriptor() const override {
    return group_->GetDescriptor();
  }
  AttributeGroupVersionId GetVersionId() const override {
    return group_->GetVersionId();
  }
  ::util::Status RegisterQuery(AttributeGroupQuery* query,
                               std::vector<Path> paths) override {
    return group_->RegisterQuery(query, std::move(paths));
  }
  void UnregisterQuery(AttributeGroupQuery* query) override {
    group_->UnregisterQuery(query);
  }

 private:
  AttributeGroupMock* group_;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ATTRIBUTE_GROUP_MOCK_H_
