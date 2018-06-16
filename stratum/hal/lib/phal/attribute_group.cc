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


#include "stratum/hal/lib/phal/attribute_group.h"

#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <tuple>
#include <vector>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/managed_attribute.h"
#include "stratum/lib/macros.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/gtl/flat_hash_map.h"
#include "stratum/glue/gtl/map_util.h"

namespace stratum {
namespace hal {
namespace phal {

// A helper class for modifying the internal protobuf inside an
// AttributeGroupQuery. An AttributeGroupQueryNode is invalid if its parent
// Query is deleted.
class AttributeGroupQueryNode {
 public:
  AttributeGroupQueryNode() {}
  explicit AttributeGroupQueryNode(AttributeGroupQuery* root_query)
      : parent_query_(root_query),
        node_(root_query->query_result_.get()),
        reflection_(node_->GetReflection()) {}
  AttributeGroupQueryNode(AttributeGroupQuery* parent_query,
                          protobuf::Message* node)
      : parent_query_(parent_query),
        node_(node),
        reflection_(node->GetReflection()) {}

  // These functions will check to make sure that adding the given field to the
  // query proto is a valid operation, but under normal circumstances this check
  // should be performed before calling this function!
  ::util::StatusOr<AttributeSetterFunction> AddAttribute(
      const std::string& name);
  ::util::StatusOr<AttributeGroupQueryNode> AddChildGroup(
      const std::string& name);
  ::util::StatusOr<AttributeGroupQueryNode> AddRepeatedChildGroup(
      const std::string& name, int idx);

  // If this is called for a child group, any AttributeGroupQueryNode referring
  // to that child group is immediately invalid.
  ::util::Status RemoveField(const std::string& name);
  void RemoveAllFields();

 private:
  ::util::StatusOr<const protobuf::FieldDescriptor*> GetFieldDescriptor(
      const std::string& name) {
    const protobuf::FieldDescriptor* descriptor =
        node_->GetDescriptor()->FindFieldByName(name);
    CHECK_RETURN_IF_FALSE(descriptor)
        << node_->GetDescriptor()->name() << " has no such field: \"" << name
        << "\".";
    return descriptor;
  }

  AttributeGroupQuery* parent_query_;
  protobuf::Message* node_;
  const protobuf::Reflection* reflection_;
};

namespace {
using protobuf::FieldDescriptor;

class AttributeGroupInternal;

// This helper class holds a lock on a AttributeGroupInternal during
// a sequence of accesses (reads or modifications).
class LockedAttributeGroup : public MutableAttributeGroup {
 public:
  // group: The group which should be locked, and to which all function calls
  //        will be passed.
  // writer: If true, acquire a writer lock. If false, this should be handled
  //         as a ReaderAttributeGroup.
  LockedAttributeGroup(AttributeGroupInternal* group, bool writer);
  ~LockedAttributeGroup() override;
  // These calls are all passed along to the held group.
  bool HasAttribute(const std::string& name) const override;
  bool HasChildGroup(const std::string& name) const override;
  std::set<std::string> GetAttributeNames() const override;
  std::set<std::string> GetChildGroupNames() const override;
  std::set<std::string> GetRepeatedChildGroupNames() const override;
  ::util::StatusOr<int> GetRepeatedChildGroupSize(
      const std::string& name) const override;
  const protobuf::Descriptor* GetDescriptor() const override;
  AttributeGroupVersionId GetVersionId() const override;
  ::util::StatusOr<ManagedAttribute*> GetAttribute(
      const std::string& name) const override;
  ::util::StatusOr<AttributeGroup*> GetChildGroup(
      const std::string& name) const override;
  ::util::StatusOr<AttributeGroup*> GetRepeatedChildGroup(
      const std::string& name, int idx) const override;
  // These must only be called if writer == true during construction.
  ::util::Status AddAttribute(const std::string& name,
                              ManagedAttribute* value) override;
  ::util::StatusOr<AttributeGroup*> AddChildGroup(
      const std::string& name) override;
  ::util::StatusOr<AttributeGroup*> AddRepeatedChildGroup(
      const std::string& name) override;
  ::util::Status RemoveAttribute(const std::string& name) override;
  ::util::Status RemoveChildGroup(const std::string& name) override;
  ::util::Status RemoveRepeatedChildGroup(const std::string& name) override;
  void AddRuntimeConfigurator(
      std::unique_ptr<AttributeGroup::RuntimeConfiguratorInterface>
          configurator) override;

  ::util::Status RegisterQuery(AttributeGroupQuery* query,
                               std::vector<Path> paths) override;
  void UnregisterQuery(AttributeGroupQuery* query) override;

 private:
  AttributeGroupInternal* group_;
  bool writer_;
};

class AttributeGroupInternal : public AttributeGroup,
                               public MutableAttributeGroup {
 public:
  explicit AttributeGroupInternal(const protobuf::Descriptor* descriptor)
      : descriptor_(descriptor) {}

  std::unique_ptr<ReadableAttributeGroup> AcquireReadable() override {
    return absl::make_unique<LockedAttributeGroup>(this, false);
  }
  std::unique_ptr<MutableAttributeGroup> AcquireMutable() override {
    return absl::make_unique<LockedAttributeGroup>(this, true);
  }

  // Mutator functions:
  ::util::Status AddAttribute(const std::string& name, ManagedAttribute* value);
  ::util::StatusOr<AttributeGroup*> AddChildGroup(const std::string& name);
  ::util::StatusOr<AttributeGroup*> AddRepeatedChildGroup(
      const std::string& name);
  ::util::Status RemoveAttribute(const std::string& name);
  ::util::Status RemoveChildGroup(const std::string& name);
  ::util::Status RemoveRepeatedChildGroup(const std::string& name);
  void AddRuntimeConfigurator(
      std::unique_ptr<RuntimeConfiguratorInterface> configurator);

  // Accessor functions:
  ::util::StatusOr<ManagedAttribute*> GetAttribute(
      const std::string& name) const;
  ::util::StatusOr<AttributeGroup*> GetChildGroup(
      const std::string& name) const;
  ::util::StatusOr<AttributeGroup*> GetRepeatedChildGroup(
      const std::string& name, int idx) const;

  bool HasAttribute(const std::string& name) const;
  bool HasChildGroup(const std::string& name) const;
  std::set<std::string> GetAttributeNames() const;
  std::set<std::string> GetChildGroupNames() const;
  std::set<std::string> GetRepeatedChildGroupNames() const;
  ::util::StatusOr<int> GetRepeatedChildGroupSize(
      const std::string& name) const;

  const protobuf::Descriptor* GetDescriptor() const { return descriptor_; }
  AttributeGroupVersionId GetVersionId() const { return version_id_; }

  ::util::Status RegisterQuery(AttributeGroupQuery* query,
                               std::vector<Path> paths) override;
  ::util::Status TraverseQuery(
      AttributeGroupQuery* query,
      std::function<
          ::util::Status(std::unique_ptr<ReadableAttributeGroup> group)>
          group_function,
      std::function<::util::Status(ManagedAttribute* attribute,
                                   const AttributeSetterFunction& setter)>
          attribute_function) override;
  void UnregisterQuery(AttributeGroupQuery* query) override;

  // TODO: move access_lock_ out of the public section.
  absl::Mutex access_lock_;

 private:
  ::util::StatusOr<const FieldDescriptor*> GetField(
      const std::string& name) const;
  template <typename T>
  ::util::Status AttemptAddAttribute(const std::string& name,
                                     ManagedAttribute* value);
  // UpdateVersionId must be called every time any structural changes are made
  // to this attribute group. It also must not be called from any accessor
  // functions; doing so would require additional locking for version_id_.
  void UpdateVersionId() { version_id_++; }

  // Stores information about a single database query that traverses this group.
  // This information is used when traversing the query's paths, as well as when
  // changing the structure of this group.
  struct RegisteredQuery {
    std::vector<Path> paths;
    bool query_all_fields = false;
    AttributeGroupQueryNode query_node;
    std::set<AttributeGroupInternal*> registered_child_groups;
    std::map<ManagedAttribute*, AttributeSetterFunction> registered_attributes;
  };
  // These helper functions check if the given query is supposed to query the
  // given attribute or attribute group, and store this information for future
  // query traversals. RegisterQueryChild and RegisterQueryRepeatedChild do this
  // recursively.
  ::util::Status RegisterQueryAttribute(RegisteredQuery* query_info,
                                        ManagedAttribute* attribute,
                                        const std::string& name);
  ::util::Status RegisterQueryChild(AttributeGroupQuery* query,
                                    RegisteredQuery* query_info,
                                    AttributeGroupInternal* group,
                                    const std::string& name);
  ::util::Status RegisterQueryRepeatedChild(AttributeGroupQuery* query,
                                            RegisteredQuery* query_info,
                                            AttributeGroupInternal* group,
                                            int idx, const std::string& name);
  // Returns a failure if the given query does not describe a valid subset of
  // the database schema proto. This validates the whole query, including parts
  // that are currently missing from the attribute database.
  ::util::Status ValidateQuery(const std::vector<Path>& paths);
  ::util::Status RegisterQueryInternal(AttributeGroupQuery* query,
                                       AttributeGroupQueryNode query_node,
                                       const std::vector<Path>& paths,
                                       bool query_all);

  // Store a count of the number of attributes in this group that are owned by
  // each datasource. Whenever one of these counts hits zero, we can remove the
  // corresponding datasource from this map.
  gtl::flat_hash_map<std::shared_ptr<DataSource>, int> required_data_sources_;
  const protobuf::Descriptor* descriptor_;
  std::map<std::string, ManagedAttribute*> attributes_;
  std::map<std::string, std::unique_ptr<AttributeGroupInternal>> sub_groups_;
  std::map<std::string, std::vector<std::unique_ptr<AttributeGroupInternal>>>
      repeated_sub_groups_;
  std::vector<std::unique_ptr<RuntimeConfiguratorInterface>>
      runtime_configurators_;
  AttributeGroupVersionId version_id_ = 0;

  absl::Mutex registered_query_lock_;
  std::map<AttributeGroupQuery*, RegisteredQuery> registered_queries_
      GUARDED_BY(registered_query_lock_);
};
}  // namespace

// Defines a setter function for a single type of attribute. This macro
// substantially reduces the size of the switch in AdAttribute. Note that this
// macro can *only* be used in AddAttribute, since it pulls in variables
// not explicitly declared as macro parameters (all of the variables captured by
// the lambda). See usage below for context.
#define ATTRIBUTE_SETTER_FUNCTION(proto_setter_function, type)               \
  AttributeSetterFunction([this, field](Attribute value) -> ::util::Status { \
    auto typed_value = absl::get_if<type>(&value);                           \
    CHECK_RETURN_IF_FALSE(typed_value)                                       \
        << "Found mismatched types for an attribute database field. "        \
        << "This indicates serious attribute database corruption.";          \
    reflection_->proto_setter_function(this->node_, field, *typed_value);    \
    /* Lambda returns success. */                                            \
    return ::util::OkStatus();                                               \
  })

::util::StatusOr<AttributeSetterFunction> AttributeGroupQueryNode::AddAttribute(
    const std::string& name) {
  absl::MutexLock lock(&parent_query_->query_lock_);
  ASSIGN_OR_RETURN(auto field, GetFieldDescriptor(name));
  CHECK_RETURN_IF_FALSE(field->cpp_type() !=
                        protobuf::FieldDescriptor::CppType::CPPTYPE_MESSAGE)
      << "Attempted to query \"" << name
      << "\" as an attribute, but it's an attribute group. This shouldn't "
         "happen!";
  // Now return a function that will set this node in the attribute database.
  switch (field->cpp_type()) {
    case FieldDescriptor::CppType::CPPTYPE_INT32:
      return ATTRIBUTE_SETTER_FUNCTION(SetInt32, int32);
    case FieldDescriptor::CppType::CPPTYPE_INT64:
      return ATTRIBUTE_SETTER_FUNCTION(SetInt64, int64);
    case FieldDescriptor::CppType::CPPTYPE_UINT32:
      return ATTRIBUTE_SETTER_FUNCTION(SetUInt32, uint32);
    case FieldDescriptor::CppType::CPPTYPE_UINT64:
      return ATTRIBUTE_SETTER_FUNCTION(SetUInt64, uint64);
    case FieldDescriptor::CppType::CPPTYPE_FLOAT:
      return ATTRIBUTE_SETTER_FUNCTION(SetFloat, float);
    case FieldDescriptor::CppType::CPPTYPE_DOUBLE:
      return ATTRIBUTE_SETTER_FUNCTION(SetDouble, double);
    case FieldDescriptor::CppType::CPPTYPE_BOOL:
      return ATTRIBUTE_SETTER_FUNCTION(SetBool, bool);
    case FieldDescriptor::CppType::CPPTYPE_STRING:
      return ATTRIBUTE_SETTER_FUNCTION(SetString, std::string);
    case FieldDescriptor::CppType::CPPTYPE_ENUM:
      return ATTRIBUTE_SETTER_FUNCTION(SetEnum,
                                       const protobuf::EnumValueDescriptor*);
    default:
      RETURN_ERROR() << "Invalid protobuf field type passed to "
                     << "QuerySingleAttribute!";
  }
}

#undef ATTRIBUTE_SETTER_FUNCTION

::util::StatusOr<AttributeGroupQueryNode>
AttributeGroupQueryNode::AddChildGroup(const std::string& name) {
  absl::MutexLock lock(&parent_query_->query_lock_);
  ASSIGN_OR_RETURN(auto field, GetFieldDescriptor(name));
  CHECK_RETURN_IF_FALSE(
      field->cpp_type() ==
          protobuf::FieldDescriptor::CppType::CPPTYPE_MESSAGE &&
      !field->is_repeated())
      << "Called AddChildGroup for \"" << name
      << "\", which is not a singular child group. This shouldn't happen!";
  return AttributeGroupQueryNode(parent_query_,
                                 reflection_->MutableMessage(node_, field));
}

::util::StatusOr<AttributeGroupQueryNode>
AttributeGroupQueryNode::AddRepeatedChildGroup(const std::string& name,
                                               int idx) {
  absl::MutexLock lock(&parent_query_->query_lock_);
  ASSIGN_OR_RETURN(auto field, GetFieldDescriptor(name));
  CHECK_RETURN_IF_FALSE(
      field->cpp_type() ==
          protobuf::FieldDescriptor::CppType::CPPTYPE_MESSAGE &&
      field->is_repeated())
      << "Called AddChildGroup for \"" << name
      << "\", which is not a repeated child group. This shouldn't happen!";
  // Add to the repeated child group until the given index is available.
  int current_field_count = reflection_->FieldSize(*node_, field);
  for (int i = current_field_count; i <= idx; i++)
    reflection_->AddMessage(node_, field);
  return AttributeGroupQueryNode(
      parent_query_, reflection_->MutableRepeatedMessage(node_, field, idx));
}

::util::Status AttributeGroupQueryNode::RemoveField(const std::string& name) {
  absl::MutexLock lock(&parent_query_->query_lock_);
  ASSIGN_OR_RETURN(auto field, GetFieldDescriptor(name));
  reflection_->ClearField(node_, field);
  return ::util::OkStatus();
}

void AttributeGroupQueryNode::RemoveAllFields() {
  absl::MutexLock lock(&parent_query_->query_lock_);
  node_->Clear();
}

::util::Status AttributeGroupQuery::Get(protobuf::Message* out) {
  std::queue<std::unique_ptr<ReadableAttributeGroup>> group_locks;
  std::map<
      DataSource*,
      std::vector<std::pair<ManagedAttribute*, const AttributeSetterFunction*>>>
      datasources;
  RETURN_IF_ERROR(root_group_->TraverseQuery(
      this,
      [&group_locks](std::unique_ptr<ReadableAttributeGroup> group) mutable {
        group_locks.push(std::move(group));
        return ::util::OkStatus();
      },
      [&datasources](ManagedAttribute* attribute,
                     const AttributeSetterFunction& setter) mutable {
        datasources[attribute->GetDataSource()].push_back({attribute, &setter});
        return ::util::OkStatus();
      }));
  // We now hold locks on all of the attribute groups relevant to this query,
  // and have a list of all the datasources and attributes we'll need to touch.
  // We can now execute our query in a threadpool.
  ::util::Status output_status;
  absl::Mutex output_status_lock;
  {
    // We acquire our query lock to avoid messy interleaving with other calls to
    // Get().
    absl::MutexLock l(&query_lock_);
    threadpool_->Start();
    std::vector<TaskId> task_ids(datasources.size());
    for (auto& datasource_and_attributes : datasources) {
      task_ids.push_back(threadpool_->Schedule([&]() {
        ::util::Status update_status =
            datasource_and_attributes.first->UpdateValuesAndLock();
        if (update_status.ok()) {
          for (auto& attribute_and_setter : datasource_and_attributes.second) {
            update_status = (*attribute_and_setter.second)(
                attribute_and_setter.first->GetValue());
          }
        }
        if (!update_status.ok()) {
          absl::MutexLock l(&output_status_lock);
          APPEND_STATUS_IF_ERROR(output_status, update_status);
        }
        datasource_and_attributes.first->Unlock();
      }));
    }
    threadpool_->WaitAll(task_ids);
    out->CopyFrom(*query_result_);
  }
  while (!group_locks.empty()) group_locks.pop();
  return output_status;
}

::util::Status AttributeGroupQuery::Subscribe(
    std::unique_ptr<ChannelWriter<PhalDB>> subscriber,
    absl::Duration polling_interval) {
  // TODO: Implement.
  RETURN_ERROR() << "AttributeGroupQuery::Subscribe is not yet implemented.";
}

::util::StatusOr<const FieldDescriptor*> AttributeGroupInternal::GetField(
    const std::string& name) const {
  auto field_descriptor = descriptor_->FindFieldByName(name);
  if (field_descriptor == nullptr) {
    RETURN_ERROR() << "No such field \"" << name << "\" in protobuf "
                   << descriptor_->name() << ".";
  }
  return field_descriptor;
}

template <typename T>
::util::Status AttributeGroupInternal::AttemptAddAttribute(
    const std::string& name, ManagedAttribute* value) {
  if (!value->GetValue().is<T>()) {
    RETURN_ERROR() << "Attempted to assign incorrect type to attribute " << name
                   << ".";
  }
  // Acquire partial ownership over this attribute's datasource by adding it to
  // our required_data_sources_.
  DataSource* datasource = value->GetDataSource();
  if (datasource == nullptr) {
    RETURN_ERROR() << "Attempted to add attribute " << name << " with no "
                   << "associated datasource.";
  }
  auto datasource_ptr = datasource->GetSharedPointer();
  // If datasource_ptr is not in required_data_sources_, this defaults to 0
  // before incrementing.
  required_data_sources_[datasource_ptr]++;
  attributes_[name] = value;
  UpdateVersionId();
  absl::WriterMutexLock lock(&registered_query_lock_);
  for (auto& query_info : registered_queries_) {
    RETURN_IF_ERROR(RegisterQueryAttribute(&query_info.second, value, name));
  }
  return ::util::OkStatus();
}

::util::Status AttributeGroupInternal::AddAttribute(const std::string& name,
                                                    ManagedAttribute* value) {
  if (attributes_.find(name) != attributes_.end()) {
    RETURN_IF_ERROR(RemoveAttribute(name))
        << "Unexpected error when removing the old definition of attribute \""
        << name << "\".";
  }
  ASSIGN_OR_RETURN(auto field, GetField(name));
  switch (field->cpp_type()) {
    case FieldDescriptor::CppType::CPPTYPE_INT32:
      return AttemptAddAttribute<int32>(name, value);
    case FieldDescriptor::CppType::CPPTYPE_INT64:
      return AttemptAddAttribute<int64>(name, value);
    case FieldDescriptor::CppType::CPPTYPE_UINT32:
      return AttemptAddAttribute<uint32>(name, value);
    case FieldDescriptor::CppType::CPPTYPE_UINT64:
      return AttemptAddAttribute<uint64>(name, value);
    case FieldDescriptor::CppType::CPPTYPE_FLOAT:
      return AttemptAddAttribute<float>(name, value);
    case FieldDescriptor::CppType::CPPTYPE_DOUBLE:
      return AttemptAddAttribute<double>(name, value);
    case FieldDescriptor::CppType::CPPTYPE_BOOL:
      return AttemptAddAttribute<bool>(name, value);
    case FieldDescriptor::CppType::CPPTYPE_STRING:
      return AttemptAddAttribute<std::string>(name, value);
    case FieldDescriptor::CppType::CPPTYPE_ENUM:
      // In addition to checking that the given ManagedAttribute is
      // an enum, we also need to check that it has a compatible enum type.
      if (!value->GetValue().is<const protobuf::EnumValueDescriptor*>()) {
        RETURN_ERROR() << "Attempted to assign non-enum type to enum "
                       << "attribute " << name << ".";
      }
      if ((*value->GetValue().get<const protobuf::EnumValueDescriptor*>())
              ->type() != field->enum_type()) {
        RETURN_ERROR() << "Attempted to assign incorrect enum type to " << name
                       << ".";
      }
      return AttemptAddAttribute<const protobuf::EnumValueDescriptor*>(name,
                                                                       value);
    default:
      RETURN_ERROR() << "Field " << name << " has unexpected type.";
  }
}

::util::StatusOr<AttributeGroup*> AttributeGroupInternal::AddChildGroup(
    const std::string& name) {
  ASSIGN_OR_RETURN(auto field, GetField(name));
  if (field->cpp_type() != FieldDescriptor::CppType::CPPTYPE_MESSAGE) {
    RETURN_ERROR() << "Attempted to make a child group, but " << name
                   << " is an attribute.";
  }
  if (field->is_repeated()) {
    RETURN_ERROR() << "Attempted to create a singular child group in a "
                   << "repeated field. Use AddRepeatedChildGroup instead.";
  }
  const protobuf::Descriptor* sub_descriptor = field->message_type();
  AttributeGroupInternal* sub_group =
      new AttributeGroupInternal(sub_descriptor);
  auto ret =
      sub_groups_.insert(std::make_pair(name, absl::WrapUnique(sub_group)));
  // ret.second is true iff the insert succeeded (i.e. no previous entry in
  // the map used key = name)
  if (!ret.second) {
    RETURN_ERROR() << "Attempted to create two attribute group with name "
                   << name << ". Not a repeated field.";
  }
  UpdateVersionId();
  absl::WriterMutexLock lock(&registered_query_lock_);
  for (auto& query_info : registered_queries_) {
    RETURN_IF_ERROR(RegisterQueryChild(query_info.first, &query_info.second,
                                       sub_group, name));
  }
  return sub_group;
}

::util::StatusOr<AttributeGroup*> AttributeGroupInternal::AddRepeatedChildGroup(
    const std::string& name) {
  ASSIGN_OR_RETURN(auto field, GetField(name));
  if (field->cpp_type() != FieldDescriptor::CppType::CPPTYPE_MESSAGE) {
    RETURN_ERROR() << "Attempted to make a child group, but " << name
                   << " is an attribute.";
  }
  if (!field->is_repeated()) {
    RETURN_ERROR() << "Attempted to create a repeated child group in an "
                   << "unrepeated field.";
  }
  const protobuf::Descriptor* sub_descriptor = field->message_type();
  AttributeGroupInternal* sub_group =
      new AttributeGroupInternal(sub_descriptor);
  repeated_sub_groups_[name].push_back(absl::WrapUnique(sub_group));
  UpdateVersionId();
  absl::WriterMutexLock lock(&registered_query_lock_);
  for (auto& query_info : registered_queries_) {
    RETURN_IF_ERROR(RegisterQueryRepeatedChild(
        query_info.first, &query_info.second, sub_group,
        repeated_sub_groups_[name].size() - 1, name));
  }
  return sub_group;
}

::util::Status AttributeGroupInternal::RemoveAttribute(
    const std::string& name) {
  auto attribute = attributes_.find(name);
  if (attribute == attributes_.end()) {
    // There's nothing to do. Check that this request is otherwise valid.
    ASSIGN_OR_RETURN(auto field, GetField(name));
    if (field->cpp_type() == FieldDescriptor::CppType::CPPTYPE_MESSAGE) {
      RETURN_ERROR() << "Called RemoveAttribute for attribute group " << name
                     << ".";
    }
  } else {
    // Check if any other attributes in this group use the same datasource. If
    // not, we can remove it from our list of required datasources.
    std::shared_ptr<DataSource> datasource =
        attribute->second->GetDataSource()->GetSharedPointer();
    auto datasource_usage = required_data_sources_.find(datasource);
    datasource_usage->second--;
    if (datasource_usage->second == 0) {
      required_data_sources_.erase(datasource_usage);
    }
    // Remove this attribute from any queries that read it.
    absl::WriterMutexLock lock(&registered_query_lock_);
    for (auto& registered_query : registered_queries_) {
      RegisteredQuery& query = registered_query.second;
      query.registered_attributes.erase(attribute->second);
      RETURN_IF_ERROR(query.query_node.RemoveField(name));
    }
    attributes_.erase(attribute);
    UpdateVersionId();
  }
  return ::util::OkStatus();
}

::util::Status AttributeGroupInternal::RemoveChildGroup(
    const std::string& name) {
  auto group = sub_groups_.find(name);
  if (group == sub_groups_.end()) {
    // There's nothing to do. Check that this request is otherwise valid.
    ASSIGN_OR_RETURN(auto field, GetField(name));
    if (field->cpp_type() != FieldDescriptor::CppType::CPPTYPE_MESSAGE) {
      RETURN_ERROR() << "Called RemoveChildGroup for attribute " << name << ".";
    } else if (field->is_repeated()) {
      RETURN_ERROR() << "Called RemoveChildGroup for repeated field " << name;
    }
  } else {
    // Remove this attribute group from any queries that read it.
    absl::WriterMutexLock lock(&registered_query_lock_);
    for (auto& registered_query : registered_queries_) {
      RegisteredQuery& query = registered_query.second;
      query.registered_child_groups.erase(group->second.get());
      RETURN_IF_ERROR(query.query_node.RemoveField(name));
    }
    sub_groups_.erase(group);
    UpdateVersionId();
  }
  return ::util::OkStatus();
}

::util::Status AttributeGroupInternal::RemoveRepeatedChildGroup(
    const std::string& name) {
  auto repeated_group = repeated_sub_groups_.find(name);
  if (repeated_group == repeated_sub_groups_.end()) {
    // There's nothing to do. Check that this request is otherwise valid.
    ASSIGN_OR_RETURN(auto field, GetField(name));
    if (field->cpp_type() != FieldDescriptor::CppType::CPPTYPE_MESSAGE) {
      RETURN_ERROR() << "Called RemoveRepeatedChildGroup for attribute " << name
                     << ".";
    } else if (!field->is_repeated()) {
      RETURN_ERROR() << "Called RemoveRepeatedChildGroup for singular field "
                     << name;
    }
  } else {
    // Remove this repeated attribute group from any queries that read it.
    absl::WriterMutexLock lock(&registered_query_lock_);
    for (auto& registered_query : registered_queries_) {
      RegisteredQuery& query = registered_query.second;
      for (auto& group : repeated_group->second) {
        query.registered_child_groups.erase(group.get());
      }
      RETURN_IF_ERROR(query.query_node.RemoveField(name));
    }
    repeated_sub_groups_.erase(repeated_group);
    UpdateVersionId();
  }
  return ::util::OkStatus();
}

void AttributeGroupInternal::AddRuntimeConfigurator(
    std::unique_ptr<RuntimeConfiguratorInterface> configurator) {
  runtime_configurators_.push_back(std::move(configurator));
}

::util::StatusOr<ManagedAttribute*> AttributeGroupInternal::GetAttribute(
    const std::string& name) const {
  auto value = attributes_.find(name);
  if (value == attributes_.end())
    RETURN_ERROR() << "Could not find requested attribute " << name;
  return value->second;
}

::util::StatusOr<AttributeGroup*> AttributeGroupInternal::GetChildGroup(
    const std::string& name) const {
  auto group = sub_groups_.find(name);
  if (group == sub_groups_.end()) {
    if (repeated_sub_groups_.find(name) != repeated_sub_groups_.end()) {
      RETURN_ERROR() << "Called GetChildGroup for repeated field " << name;
    }
    RETURN_ERROR() << "Could not find requested attribute group " << name;
  }
  return group->second.get();
}

::util::StatusOr<AttributeGroup*> AttributeGroupInternal::GetRepeatedChildGroup(
    const std::string& name, int idx) const {
  auto group_list = repeated_sub_groups_.find(name);
  if (group_list == repeated_sub_groups_.end()) {
    if (sub_groups_.find(name) != sub_groups_.end()) {
      RETURN_ERROR() << "Called GetRepeatedChildGroup for singular group "
                     << name;
    } else {
      RETURN_ERROR() << "Could not find requested repeated attribute group "
                     << name;
    }
  }
  if (idx < 0 || static_cast<size_t>(idx) >= group_list->second.size()) {
    RETURN_ERROR() << "Invalid index " << idx << " in repeated field " << name
                   << " with " << group_list->second.size() << " elements.";
  }
  return group_list->second[idx].get();
}

bool AttributeGroupInternal::HasAttribute(const std::string& name) const {
  return attributes_.find(name) != attributes_.end();
}

bool AttributeGroupInternal::HasChildGroup(const std::string& name) const {
  return sub_groups_.find(name) != sub_groups_.end();
}

std::set<std::string> AttributeGroupInternal::GetAttributeNames() const {
  std::set<std::string> names;
  for (auto const& entry : attributes_) names.insert(entry.first);
  return names;
}

std::set<std::string> AttributeGroupInternal::GetChildGroupNames() const {
  std::set<std::string> names;
  for (auto const& entry : sub_groups_) names.insert(entry.first);
  return names;
}

std::set<std::string> AttributeGroupInternal::GetRepeatedChildGroupNames()
    const {
  std::set<std::string> names;
  for (auto const& entry : repeated_sub_groups_) names.insert(entry.first);
  return names;
}

::util::StatusOr<int> AttributeGroupInternal::GetRepeatedChildGroupSize(
    const std::string& name) const {
  auto group_list = repeated_sub_groups_.find(name);
  if (group_list == repeated_sub_groups_.end()) {
    ASSIGN_OR_RETURN(auto field, GetField(name));
    if (field->cpp_type() != FieldDescriptor::CppType::CPPTYPE_MESSAGE) {
      RETURN_ERROR() << "Called GetRepeatedChildGroupSize for attribute \""
                     << name << "\".";
    }
    if (field->is_repeated()) {
      // This is a repeated child group that's never been used.
      return 0;
    } else {
      RETURN_ERROR()
          << "Called GetRepeatedChildGroupSize for singular child group \""
          << name << "\".";
    }
  }
  return group_list->second.size();
}

::util::Status AttributeGroupInternal::RegisterQueryAttribute(
    RegisteredQuery* query_info, ManagedAttribute* attribute,
    const std::string& name) {
  bool query_applies = query_info->query_all_fields;
  for (const auto& path : query_info->paths) {
    if (path.empty()) {
      RETURN_ERROR() << "Should never encounter a zero length path.";
    } else if (path.size() == 1 && !path[0].terminal_group &&
               path[0].name == name) {
      query_applies = true;
    }
  }
  if (query_applies) {
    ASSIGN_OR_RETURN(auto setter_function,
                     query_info->query_node.AddAttribute(name));
    query_info->registered_attributes.insert({attribute, setter_function});
  }
  return ::util::OkStatus();
}

::util::Status AttributeGroupInternal::RegisterQueryChild(
    AttributeGroupQuery* query, RegisteredQuery* query_info,
    AttributeGroupInternal* group, const std::string& name) {
  bool query_applies = query_info->query_all_fields;
  bool query_all_subfields = query_info->query_all_fields;
  std::vector<Path> query_paths;
  for (const auto& path : query_info->paths) {
    if (path.empty()) {
      RETURN_ERROR() << "Should never encounter a zero length path.";
    } else if (path.size() > 1 && !path[0].indexed && path[0].name == name) {
      query_applies = true;
      query_paths.push_back({path.begin() + 1, path.end()});
    } else if (path.size() == 1 && path[0].terminal_group &&
               path[0].name == name) {
      query_applies = true;
      query_all_subfields = true;
    }
  }
  if (query_applies) {
    auto group_lock = group->AcquireReadable();
    ASSIGN_OR_RETURN(auto sub_node, query_info->query_node.AddChildGroup(name));
    RETURN_IF_ERROR(group->RegisterQueryInternal(query, sub_node, query_paths,
                                                 query_all_subfields));
    query_info->registered_child_groups.insert(group);
  }
  return ::util::OkStatus();
}

::util::Status AttributeGroupInternal::RegisterQueryRepeatedChild(
    AttributeGroupQuery* query, RegisteredQuery* query_info,
    AttributeGroupInternal* group, int idx, const std::string& name) {
  bool query_applies = query_info->query_all_fields;
  bool query_all_subfields = query_info->query_all_fields;
  std::vector<Path> query_paths;
  for (const auto& path : query_info->paths) {
    if (path.empty()) {
      RETURN_ERROR() << "Should never encounter a zero length path.";
    } else if (path.size() > 1 && path[0].indexed && path[0].name == name &&
               (path[0].all || path[0].index == idx)) {
      query_applies = true;
      query_paths.push_back({path.begin() + 1, path.end()});
    } else if (path.size() == 1 && path[0].terminal_group &&
               path[0].name == name) {
      query_applies = true;
      query_all_subfields = true;
    }
  }
  if (query_applies) {
    auto group_lock = group->AcquireReadable();
    ASSIGN_OR_RETURN(auto sub_node,
                     query_info->query_node.AddRepeatedChildGroup(name, idx));
    RETURN_IF_ERROR(group->RegisterQueryInternal(query, sub_node, query_paths,
                                                 query_all_subfields));
    query_info->registered_child_groups.insert(group);
  }
  return ::util::OkStatus();
}

::util::Status AttributeGroupInternal::RegisterQueryInternal(
    AttributeGroupQuery* query, AttributeGroupQueryNode query_node,
    const std::vector<Path>& paths, bool query_all) {
  absl::WriterMutexLock lock(&registered_query_lock_);
  RegisteredQuery* query_info = &registered_queries_[query];
  query_info->paths = paths;
  query_info->query_all_fields |= query_all;
  query_info->query_node = query_node;
  for (auto& attribute : attributes_) {
    RETURN_IF_ERROR(
        RegisterQueryAttribute(query_info, attribute.second, attribute.first));
  }
  for (auto& child_group : sub_groups_) {
    RETURN_IF_ERROR(RegisterQueryChild(
        query, query_info, child_group.second.get(), child_group.first));
  }
  for (auto& repeated_child_group : repeated_sub_groups_) {
    const std::string& group_name = repeated_child_group.first;
    const std::vector<std::unique_ptr<AttributeGroupInternal>>& group_fields =
        repeated_child_group.second;
    for (int i = 0; i < group_fields.size(); i++) {
      RETURN_IF_ERROR(RegisterQueryRepeatedChild(
          query, query_info, group_fields[i].get(), i, group_name));
    }
  }
  return ::util::OkStatus();
}

::util::Status AttributeGroupInternal::ValidateQuery(
    const std::vector<Path>& paths) {
  for (const auto& path : paths) {
    const protobuf::Descriptor* descriptor = descriptor_;
    for (int i = 0; i < path.size(); i++) {
      const PathEntry& entry = path[i];
      const FieldDescriptor* field = descriptor->FindFieldByName(entry.name);
      CHECK_RETURN_IF_FALSE(field)
          << "No such field \"" << entry.name << "\" in attribute group \""
          << descriptor->name() << "\".";
      bool field_is_child_group =
          field->cpp_type() == FieldDescriptor::CppType::CPPTYPE_MESSAGE;
      if (i == path.size() - 1) {
        if (field_is_child_group) {
          CHECK_RETURN_IF_FALSE(entry.terminal_group)
              << "Encountered a query path ending in the attribute group \""
              << entry.name << "\", but not marked as a terminal group.";
        } else {
          CHECK_RETURN_IF_FALSE(!entry.terminal_group)
              << "Encountered a query path that marks the attribute \""
              << entry.name << "\" as a terminal group.";
        }
      } else {
        CHECK_RETURN_IF_FALSE(field_is_child_group)
            << "Encountered the attribute \"" << entry.name
            << "\" somewhere other than the last position of a query path.";
        CHECK_RETURN_IF_FALSE(!entry.terminal_group)
            << "Encountered the terminal attribute group \"" << entry.name
            << "\" somewhere other than the last position of a query path.";
        if (entry.indexed) {
          CHECK_RETURN_IF_FALSE(field->is_repeated())
              << "Query path entry is marked as indexed, but \"" << entry.name
              << "\" is a singular attribute group.";
          CHECK_RETURN_IF_FALSE(entry.all || entry.index >= 0)
              << "Encountered an indexed query path with a negative index.";
        } else {
          CHECK_RETURN_IF_FALSE(!field->is_repeated())
              << "Query path entry  is not marked as indexed, but \""
              << entry.name << "\" is a repeated attribute group.";
        }
      }
      if (field_is_child_group) descriptor = field->message_type();
    }
  }
  return ::util::OkStatus();
}

::util::Status AttributeGroupInternal::RegisterQuery(AttributeGroupQuery* query,
                                                     std::vector<Path> paths) {
  RETURN_IF_ERROR(ValidateQuery(paths));
  return RegisterQueryInternal(query, AttributeGroupQueryNode(query), paths,
                               false);
}

::util::Status AttributeGroupInternal::TraverseQuery(
    AttributeGroupQuery* query,
    std::function<::util::Status(std::unique_ptr<ReadableAttributeGroup> group)>
        group_function,
    std::function<::util::Status(ManagedAttribute* attribute,
                                 const AttributeSetterFunction& setter)>
        attribute_function) {
  auto reader_lock = AcquireReadable();
  absl::ReaderMutexLock lock(&registered_query_lock_);
  auto query_info = gtl::FindOrNull(registered_queries_, query);
  CHECK_RETURN_IF_FALSE(query_info)
      << "Attempted to traverse a query that is not registered with this "
         "attribute group.";
  for (auto& child_group : query_info->registered_child_groups) {
    RETURN_IF_ERROR(
        child_group->TraverseQuery(query, group_function, attribute_function));
  }
  for (auto& attribute_and_setter : query_info->registered_attributes) {
    RETURN_IF_ERROR(attribute_function(attribute_and_setter.first,
                                       attribute_and_setter.second));
  }
  return group_function(std::move(reader_lock));
}

void AttributeGroupInternal::UnregisterQuery(AttributeGroupQuery* query) {
  absl::WriterMutexLock lock(&registered_query_lock_);
  auto query_info = gtl::FindOrNull(registered_queries_, query);
  if (!query_info) return;
  for (auto child_group : query_info->registered_child_groups) {
    child_group->AcquireReadable()->UnregisterQuery(query);
  }
  query_info->query_node.RemoveAllFields();
  registered_queries_.erase(query);
}

std::unique_ptr<AttributeGroup> AttributeGroup::From(
    const protobuf::Descriptor* descriptor) {
  return absl::make_unique<AttributeGroupInternal>(descriptor);
}

LockedAttributeGroup::LockedAttributeGroup(AttributeGroupInternal* group,
                                           bool writer)
    : group_(group), writer_(writer) {
  if (writer) {
    group_->access_lock_.WriterLock();
  } else {
    group_->access_lock_.ReaderLock();
  }
}

LockedAttributeGroup::~LockedAttributeGroup() {
  if (writer_) {
    group_->access_lock_.WriterUnlock();
  } else {
    group_->access_lock_.ReaderUnlock();
  }
}

// Pass all calls through to the held group.
::util::StatusOr<ManagedAttribute*> LockedAttributeGroup::GetAttribute(
    const std::string& name) const {
  return group_->GetAttribute(name);
}
::util::StatusOr<AttributeGroup*> LockedAttributeGroup::GetChildGroup(
    const std::string& name) const {
  return group_->GetChildGroup(name);
}
::util::StatusOr<AttributeGroup*> LockedAttributeGroup::GetRepeatedChildGroup(
    const std::string& name, int idx) const {
  return group_->GetRepeatedChildGroup(name, idx);
}
bool LockedAttributeGroup::HasAttribute(const std::string& name) const {
  return group_->HasAttribute(name);
}
bool LockedAttributeGroup::HasChildGroup(const std::string& name) const {
  return group_->HasChildGroup(name);
}
std::set<std::string> LockedAttributeGroup::GetAttributeNames() const {
  return group_->GetAttributeNames();
}
std::set<std::string> LockedAttributeGroup::GetChildGroupNames() const {
  return group_->GetChildGroupNames();
}
std::set<std::string> LockedAttributeGroup::GetRepeatedChildGroupNames() const {
  return group_->GetRepeatedChildGroupNames();
}
::util::StatusOr<int> LockedAttributeGroup::GetRepeatedChildGroupSize(
    const std::string& name) const {
  return group_->GetRepeatedChildGroupSize(name);
}
const protobuf::Descriptor* LockedAttributeGroup::GetDescriptor() const {
  return group_->GetDescriptor();
}
AttributeGroupVersionId LockedAttributeGroup::GetVersionId() const {
  return group_->GetVersionId();
}
::util::Status LockedAttributeGroup::AddAttribute(const std::string& name,
                                                  ManagedAttribute* value) {
  return group_->AddAttribute(name, value);
}
::util::StatusOr<AttributeGroup*> LockedAttributeGroup::AddChildGroup(
    const std::string& name) {
  return group_->AddChildGroup(name);
}
::util::StatusOr<AttributeGroup*> LockedAttributeGroup::AddRepeatedChildGroup(
    const std::string& name) {
  return group_->AddRepeatedChildGroup(name);
}
::util::Status LockedAttributeGroup::RemoveAttribute(const std::string& name) {
  return group_->RemoveAttribute(name);
}
::util::Status LockedAttributeGroup::RemoveChildGroup(const std::string& name) {
  return group_->RemoveChildGroup(name);
}
::util::Status LockedAttributeGroup::RemoveRepeatedChildGroup(
    const std::string& name) {
  return group_->RemoveRepeatedChildGroup(name);
}

void LockedAttributeGroup::AddRuntimeConfigurator(
    std::unique_ptr<AttributeGroup::RuntimeConfiguratorInterface>
        configurator) {
  group_->AddRuntimeConfigurator(std::move(configurator));
}
::util::Status LockedAttributeGroup::RegisterQuery(AttributeGroupQuery* query,
                                                   std::vector<Path> paths) {
  return group_->RegisterQuery(query, std::move(paths));
}
void LockedAttributeGroup::UnregisterQuery(AttributeGroupQuery* query) {
  group_->UnregisterQuery(query);
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
