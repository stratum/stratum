/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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


#ifndef STRATUM_HAL_LIB_PHAL_ATTRIBUTE_GROUP_H_
#define STRATUM_HAL_LIB_PHAL_ATTRIBUTE_GROUP_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "google/protobuf/message.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/managed_attribute.h"
#include "stratum/hal/lib/phal/threadpool_interface.h"
#include "stratum/lib/macros.h"
#include "absl/synchronization/mutex.h"
#include "absl/container/flat_hash_map.h"

namespace stratum {
namespace hal {
namespace phal {

class ReadableAttributeGroup;
class MutableAttributeGroup;
class AttributeGroupQuery;

using AttributeGroupVersionId = uint32;

using AttributeSetterFunction = std::function<::util::Status(Attribute value)>;

// A single node in an AttributeDatabase. The contents of an AttributeGroup are
// required to follow the structure of a schema protobuf message.
class AttributeGroup {
 public:
  virtual ~AttributeGroup() = default;

  // A factory function to produce a AttributeGroup that uses the given protobuf
  // message as its schema.
  static std::unique_ptr<AttributeGroup> From(
      const google::protobuf::Descriptor* descriptor);

  // The following two functions lock this attribute group as appropriate,
  // and expose subsets of its interface. The returned classes are invalid
  // after this AttributeGroup is destroyed. Attempting to AcquireReadable and
  // AcquireMutable simultaneously in the same thread will result in deadlock.
  virtual std::unique_ptr<ReadableAttributeGroup> AcquireReadable() = 0;
  virtual std::unique_ptr<MutableAttributeGroup> AcquireMutable() = 0;

  // Traverses all of the attribute groups and attributes that match the given
  // query, and passes these groups and attributes into the given functions.
  // IMPORTANT NOTE: The ReadableAttributeGroups passed in *must* be deleted in
  // the order in which they are passed to avoid data races. As long as this
  // ordering is kept, it is perfectly safe for the caller to store the
  // ReadableAttributeGroups for later use or to temporarily freeze the database
  // structure.
  virtual ::util::Status TraverseQuery(
      AttributeGroupQuery* query,
      std::function<
          ::util::Status(std::unique_ptr<ReadableAttributeGroup> group)>
          group_function,
      std::function<::util::Status(ManagedAttribute* attribute,
                                   const Path& querying_path,
                                   const AttributeSetterFunction& setter)>
          attribute_function) = 0;

  virtual ::util::Status Set(const AttributeValueMap& values,
                             ThreadpoolInterface* threadpool) = 0;

  // A RuntimeConfigurator is responsible for altering the structure of an
  // attribute database at runtime. Derived classes of RuntimeConfigurator
  // handle specific cases. Two RuntimeConfigurators must *never* execute
  // simultaneously, since this would break attribute database locking rules.
  class RuntimeConfiguratorInterface {
   public:
    virtual ~RuntimeConfiguratorInterface() {}
  };

 protected:
  AttributeGroup() {}
};

class ReadableAttributeGroup {
 public:
  virtual ~ReadableAttributeGroup() {}
  virtual ::util::StatusOr<ManagedAttribute*> GetAttribute(
      const std::string& name) const = 0;
  virtual ::util::StatusOr<AttributeGroup*> GetChildGroup(
      const std::string& name) const = 0;
  virtual ::util::StatusOr<AttributeGroup*> GetRepeatedChildGroup(
      const std::string& name, int idx) const = 0;

  // Finds the attribute with the given name and reads its value. Fails if the
  // attribute does not exist or contains a value of a different type than the
  // one specified.
  template <typename T>
  ::util::StatusOr<T> ReadAttribute(const std::string& name) const {
    ManagedAttribute* attr;
    ASSIGN_OR_RETURN(attr, GetAttribute(name));
    return attr->ReadValue<T>();
  }

  // These functions check if a given attribute or child group has previously
  // been added to this group. The equivalent for a repeated child group is
  // GetRepeatedChildGroupSize(name) > 0.
  virtual bool HasAttribute(const std::string& name) const = 0;
  virtual bool HasChildGroup(const std::string& name) const = 0;
  // These functions each return a set containing every attribute or group name
  // that has been explicity added to this group (simply existing in the
  // protobuf schema is not enough). In the case of a repeated field, at least
  // one field must have been added.
  virtual std::set<std::string> GetAttributeNames() const = 0;
  virtual std::set<std::string> GetChildGroupNames() const = 0;
  virtual std::set<std::string> GetRepeatedChildGroupNames() const = 0;
  // Returns the number of fields that have been added to the given
  // repeated field. Returns 0 if the group name is valid but no child groups
  // have been added.
  virtual ::util::StatusOr<int> GetRepeatedChildGroupSize(
      const std::string& name) const = 0;

  // Returns the protobuf descriptor that constrains this attribute group. And
  // fields in this protobuf must exist in the passed descriptor.
  virtual const google::protobuf::Descriptor* GetDescriptor() const = 0;

  // Returns the current version of this attribute group. This id is changed
  // every time any structural changes are made to this attribute group.
  virtual AttributeGroupVersionId GetVersionId() const = 0;

  // Registers the given query to traverse a set of paths addressed from this
  // attribute group. This function may only be called once for a given query,
  // unless that query has also been passed to UnregisterQuery.
  virtual ::util::Status RegisterQuery(AttributeGroupQuery* query,
                                       std::vector<Path> paths) = 0;
  // Unregisters the given query, terminating any ongoing streaming queries.
  // This function may be called at any time, but will be called automatically
  // when a query is deleted.
  virtual void UnregisterQuery(AttributeGroupQuery* query) = 0;

 protected:
  ReadableAttributeGroup() {}
};

class MutableAttributeGroup : public ReadableAttributeGroup {
 public:
  // Adds the given attribute to the database iff its type and name match the
  // protobuf descriptor. The given ManagedAttribute must have an associated
  // DataSource. This attribute group acquires partial ownership over that
  // DataSource. If another attribute has already been added with the given
  // name, it is safely overwritten.
  virtual ::util::Status AddAttribute(const std::string& name,
                                      ManagedAttribute* value) = 0;
  // Adds a new child group to this group iff its name matches the protobuf
  // descriptor and it is not repeated. The returned AttributeGroup
  // can only be configured to match the structure of the corresponding message
  // field in the protobuf. Does *not* transfer ownership of the returned
  // pointer. Fails if another attribute group has already been added with the
  // given name.
  virtual ::util::StatusOr<AttributeGroup*> AddChildGroup(
      const std::string& name) = 0;
  // Identical to AddChildGroup, but may only be called for a repeated field,
  // and may be called multiple times. The groups returned by sequential calls
  // are assigned ascending indices.
  // Does *not* transfer ownership of the returned pointer.
  virtual ::util::StatusOr<AttributeGroup*> AddRepeatedChildGroup(
      const std::string& name) = 0;
  // Removes an attribute from this group if it has previously been added. Given
  // attribute must be present in this group's schema.
  virtual ::util::Status RemoveAttribute(const std::string& name) = 0;
  // Removes a child group from this group iff it has previously been added and
  // it is not repeated. This recursively deletes all of the children of the
  // specified group. The given child group must be present in this group's
  // schema.
  virtual ::util::Status RemoveChildGroup(const std::string& name) = 0;
  // Removes all of the repeated child groups that have been added under the
  // given name. The given repeated child group must be present in this group's
  // schema. Note that there is no way to remove repeated children one at a
  // time, since the index of each group can store important information. If
  // an individual repeated group should be removed, it should instead be
  // modified to reflect that it is missing.
  virtual ::util::Status RemoveRepeatedChildGroup(const std::string& name) = 0;
  // Adds a runtime configurator that will be responsible for altering this
  // attribute group at runtime. Acquires ownership of the passed runtime
  // configurator. Once added, a runtime configurator will never be deleted
  // until this attribute group is deleted.
  virtual void AddRuntimeConfigurator(
      std::unique_ptr<AttributeGroup::RuntimeConfiguratorInterface>
          configurator) = 0;
};

// A query that starts from a specific attribute group. This is nearly identical
// to a normal Query as defined in attribute_database_interface.h, but has small
// interface differences to support generic (i.e. non-PhalDB) attribute groups.
// Specific query paths may be added to an AttributeGroupQuery by
// ReadableAttributeGroup::RegisterQuery.
class AttributeGroupQuery {
 public:
  // Constructs a new query that starts from the given attribute group and uses
  // the given threadpool to parallelize database queries. RegisterQuery should
  // only be called for the given attribute group.
  AttributeGroupQuery(AttributeGroup* root_group,
                      ThreadpoolInterface* threadpool)
      : root_group_(root_group), threadpool_(threadpool) {
    auto descriptor = root_group->AcquireReadable()->GetDescriptor();
    const google::protobuf::Message* prototype_message =
    google::protobuf::MessageFactory::generated_factory()->
    GetPrototype(descriptor);
    CHECK(prototype_message != nullptr);
    query_result_.reset(prototype_message->New());
  }
  AttributeGroupQuery(const AttributeGroupQuery& other) = delete;
  AttributeGroupQuery& operator=(const AttributeGroupQuery& other) = delete;
  ~AttributeGroupQuery() {
    root_group_->AcquireReadable()->UnregisterQuery(this);
  }
  // Executes this query, and writes all of the values read from the attribute
  // database into the given output protobuf. The passed protobuf must be of the
  // same type used for the descriptor of root_group.
  ::util::Status Get(google::protobuf::Message* out)
    LOCKS_EXCLUDED(query_lock_);
  ::util::Status Subscribe(std::unique_ptr<ChannelWriter<PhalDB>> subscriber,
                           absl::Duration polling_interval)
      LOCKS_EXCLUDED(query_lock_);

  bool IsUpdated() LOCKS_EXCLUDED(query_lock_);
  void MarkUpdated() LOCKS_EXCLUDED(query_lock_);
  void ClearUpdated() LOCKS_EXCLUDED(query_lock_);

 private:
  friend class AttributeGroupQueryNode;

  AttributeGroup* root_group_;
  ThreadpoolInterface* threadpool_;
  std::unique_ptr<google::protobuf::Message> query_result_;
  absl::Mutex query_lock_;
  // If true, the result of this query has changed and a streaming message
  // should shortly be sent to all subscribers.
  bool query_updated_ GUARDED_BY(query_lock_) = false;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ATTRIBUTE_GROUP_H_
