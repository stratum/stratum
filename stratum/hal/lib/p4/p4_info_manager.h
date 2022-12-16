// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// The P4InfoManager provides convenient functions for accessing data in a
// p4.config.P4Info message.

#ifndef STRATUM_HAL_LIB_P4_P4_INFO_MANAGER_H_
#define STRATUM_HAL_LIB_P4_P4_INFO_MANAGER_H_

#include <functional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "google/protobuf/repeated_field.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/p4/utils.h"
#include "stratum/lib/macros.h"
#include "stratum/public/proto/p4_annotation.pb.h"

namespace stratum {
namespace hal {

// The P4InfoManager constructor takes one P4Info message as input.  This set
// of P4Info defines the internal state of the P4InfoManager.  Normal usage is:
//
//  P4InfoManager p4_info_mgr(<P4 configuration from external source>);
//  ::util::Status status = InitializeAndVerify();
//  <code to call p4_info_mgr lookup methods, etc.>
//
// A P4InfoManager has multiple use cases:
//  - It can be used to do translations between P4 resource names and IDs.  In
//    this use case, an initialized instance of P4InfoManager is expected to
//    remain in scope for the lifetime of the P4Info data.
//  - It can be used to verify correctness of a new set of P4Info.  For example,
//    if the controller pushes a P4Info update, verification of the new P4Info
//    can be done by creating another P4InfoManager and using the result of
//    InitializeAndVerify.
//
// A P4InfoManager has no explicit lock protection.  It becomes immutable after
// InitializeAndVerify returns, so it is safe for all threads to read following
// initialization.
class P4InfoManager {
 public:
  // The constructor makes a copy of the input p4_info, but it takes no other
  // actions.  A call to InitializeAndVerify is necessary to fully define the
  // state of this P4InfoManager.
  explicit P4InfoManager(const ::p4::config::v1::P4Info& p4_info);
  virtual ~P4InfoManager();

  // Derives all internal state and lookup maps based on p4_info_.
  // InitializeAndVerify must be called before any other method.  It normally
  // returns an OK status, but it can fail if the p4_info_ contains invalid
  // data, such as duplication of table IDs or names.  It also fails if called
  // more than once.  As InitializeAndVerify runs, it does as much as it can to
  // verify the overall correctness of its P4Info.  For example, it confirms
  // that all header field and action ID references in table definitions refer
  // to validly defined P4 resources.
  virtual ::util::Status InitializeAndVerify();

  // These methods lookup P4 resource information that corresponds to the input
  // ID or name.  A successful lookup returns a copy of the resource data
  // from the P4Info.  The lookup fails and returns an error if the requested
  // resource does not exist.
  virtual ::util::StatusOr<const ::p4::config::v1::Table> FindTableByID(
      uint32 table_id) const;
  virtual ::util::StatusOr<const ::p4::config::v1::Table> FindTableByName(
      const std::string& table_name) const;
  virtual ::util::StatusOr<const ::p4::config::v1::Action> FindActionByID(
      uint32 action_id) const;
  virtual ::util::StatusOr<const ::p4::config::v1::Action> FindActionByName(
      const std::string& action_name) const;
  virtual ::util::StatusOr<const ::p4::config::v1::ActionProfile>
  FindActionProfileByID(uint32 profile_id) const;
  virtual ::util::StatusOr<const ::p4::config::v1::ActionProfile>
  FindActionProfileByName(const std::string& profile_name) const;
  virtual ::util::StatusOr<const ::p4::config::v1::Counter> FindCounterByID(
      uint32 counter_id) const;
  virtual ::util::StatusOr<const ::p4::config::v1::Counter> FindCounterByName(
      const std::string& counter_name) const;
  virtual ::util::StatusOr<const ::p4::config::v1::DirectCounter>
  FindDirectCounterByID(uint32 counter_id) const;
  virtual ::util::StatusOr<const ::p4::config::v1::DirectCounter>
  FindDirectCounterByName(const std::string& counter_name) const;
  virtual ::util::StatusOr<const ::p4::config::v1::Meter> FindMeterByID(
      uint32 meter_id) const;
  virtual ::util::StatusOr<const ::p4::config::v1::Meter> FindMeterByName(
      const std::string& meter_name) const;
  virtual ::util::StatusOr<const ::p4::config::v1::DirectMeter>
  FindDirectMeterByID(uint32 meter_id) const;
  virtual ::util::StatusOr<const ::p4::config::v1::DirectMeter>
  FindDirectMeterByName(const std::string& meter_name) const;
  virtual ::util::StatusOr<const ::p4::config::v1::ValueSet> FindValueSetByID(
      uint32 value_set_id) const;
  virtual ::util::StatusOr<const ::p4::config::v1::ValueSet> FindValueSetByName(
      const std::string& value_set_name) const;
  virtual ::util::StatusOr<const ::p4::config::v1::Register> FindRegisterByID(
      uint32 register_id) const;
  virtual ::util::StatusOr<const ::p4::config::v1::Register> FindRegisterByName(
      const std::string& register_name) const;
  virtual ::util::StatusOr<const ::p4::config::v1::Digest> FindDigestByID(
      uint32 digest_id) const;
  virtual ::util::StatusOr<const ::p4::config::v1::Digest> FindDigestByName(
      const std::string& digest_name) const;

  // GetSwitchStackAnnotations attempts to parse any @switchstack annotations
  // in the input object's P4Info Preamble.  If the P4 object has multiple
  // @switchstack annotations, GetSwitchStackAnnotations merges them into
  // the returned message with the most recent annotation taking precedence
  // when duplicates occur (such as pipeline_stage appearing twice).  The
  // return status is OK if the annotations parse successfully or if
  // p4_object_name has no annotations.  The return status indicates an error
  // if annotations exist, but they do not parse.  An error also occurs if
  // p4_object_name does not identify a P4Info object type that contains
  // a Preamble.
  virtual ::util::StatusOr<P4Annotation> GetSwitchStackAnnotations(
      const std::string& p4_object_name) const;

  // TODO(unknown): Consider a method to verify the data in a TableWrite RPC,
  // i.e. doing a verification that all P4 resources referenced by the RPC
  // actually exist in the P4Info.

  // Verifies that a P4 RegisterEntry is well formed according to the P4Info.
  // TODO(max): Consider splitting this function into two versions, one for read
  // and write requests each. A RegisterEntry without index and data is a valid
  // read request, but not a valid write request or a read response.
  virtual ::util::Status VerifyRegisterEntry(
      const ::p4::v1::RegisterEntry& register_entry) const;

  // Verifies that the given P4Data matches the given type spec.
  virtual ::util::Status VerifyTypeSpec(
      const ::p4::v1::P4Data& data,
      const ::p4::config::v1::P4DataTypeSpec& type_spec) const;

  // Outputs LOG messages with name to ID translations for all p4_info_
  // entities.
  virtual void DumpNamesToIDs() const;

  // Accesses the P4Info - virtual for mock access.
  virtual const ::p4::config::v1::P4Info& p4_info() const { return p4_info_; }

  // P4InfoManager is neither copyable nor movable.
  P4InfoManager(const P4InfoManager&) = delete;
  P4InfoManager& operator=(const P4InfoManager&) = delete;

 protected:
  P4InfoManager();  // Default constructor for mock use only.

  // Assures that p4_info_ contains the minimum set of objects required to be
  // viable.  For most platforms, this means one or more tables, actions,
  // and header fields must be present.  Platforms with more or less restrictive
  // requirements can override VerifyRequiredObjects to suit their needs.
  virtual ::util::Status VerifyRequiredObjects();

 private:
  // This type defines a callback that verifies the Preamble content.
  typedef std::function<::util::Status(
      const ::p4::config::v1::Preamble& preamble,
      const std::string& resource_type)>
      PreambleCallback;

  // This class provides a common implementation for mapping P4 IDs and names
  // to a specific P4 resource of type T, i.e. name/ID to Table, name/ID to
  // Action, etc.
  template <class T>
  class P4ResourceMap {
   public:
    // The resource_type is a descriptive string for logging and error messages.
    explicit P4ResourceMap(const std::string& resource_type)
        : resource_type_(resource_type) {}
    virtual ~P4ResourceMap() {}

    // Iterates over all the P4 resources of type T and builds the internal
    // maps for ID and name lookup.
    ::util::Status BuildMaps(
        const ::google::protobuf::RepeatedPtrField<T>& p4_resources,
        PreambleCallback preamble_cb) {
      ::util::Status status = ::util::OkStatus();
      for (const auto& resource : p4_resources) {
        auto preamble_status = preamble_cb(resource.preamble(), resource_type_);
        if (preamble_status.ok()) {
          AddIdMapEntry(resource);
          AddNameMapEntry(resource);
        } else {
          APPEND_STATUS_IF_ERROR(status, preamble_status);
        }
      }
      return status;
    }

    // Attempts to find the P4 resource matching the input ID.
    ::util::StatusOr<const T> FindByID(uint32 id) const {
      auto iter = id_to_resource_map_.find(id);
      if (iter == id_to_resource_map_.end()) {
        return MAKE_ERROR(ERR_INVALID_P4_INFO)
               << "P4Info " << resource_type_ << " ID " << PrintP4ObjectID(id)
               << " is not found";
      }
      return *iter->second;
    }

    // Attempts to find the P4 resource matching the input name.
    ::util::StatusOr<const T> FindByName(const std::string& name) const {
      auto iter = name_to_resource_map_.find(name);
      if (iter == name_to_resource_map_.end()) {
        return MAKE_ERROR(ERR_INVALID_P4_INFO)
               << "P4Info " << resource_type_ << " name " << name
               << " is not found";
      }
      return *iter->second;
    }

    // Outputs LOG messages with name to ID translations for all members of
    // this P4ResourceMap.
    void DumpNamesToIDs() const {
      for (auto iter : name_to_resource_map_) {
        LOG(INFO) << resource_type_ << " name " << iter.first << " has ID "
                  << PrintP4ObjectID(iter.second->preamble().id());
      }
    }

    // Accessor.
    const std::string& resource_type() const { return resource_type_; }

   private:
    // The next two methods create lookup map entries for the input resource.
    // They expect that the preamble ID and name have been validated before
    // they are called.
    void AddIdMapEntry(const T& p4_resource) {
      uint32 id_key = p4_resource.preamble().id();
      auto id_result = id_to_resource_map_.emplace(id_key, &p4_resource);
      DCHECK(id_result.second)
          << "P4Info unexpected duplicate " << resource_type_ << " ID "
          << PrintP4ObjectID(id_key);
    }

    void AddNameMapEntry(const T& p4_resource) {
      const std::string& name_key = p4_resource.preamble().name();
      auto name_result = name_to_resource_map_.emplace(name_key, &p4_resource);
      DCHECK(name_result.second) << "P4Info unexpected duplicate "
                                 << resource_type_ << " name " << name_key;
    }

    const std::string resource_type_;  // String used in errors and logs.

    // These maps facilitate lookups from P4 name/ID to resource type T.
    absl::flat_hash_map<uint32, const T*> id_to_resource_map_;
    absl::flat_hash_map<std::string, const T*> name_to_resource_map_;
  };

  // Does common processing of Preamble fields embedded in any resource,
  // returning an error status if the name or ID is invalid or non-unique.
  ::util::Status ProcessPreamble(const ::p4::config::v1::Preamble& preamble,
                                 const std::string& resource_type);

  // Verifies cross-references from Tables to Actions and Header Fields.
  ::util::Status VerifyTableXrefs();

  // Functions to validate name and ID presence in message preamble.
  static ::util::Status VerifyID(const ::p4::config::v1::Preamble& preamble,
                                 const std::string& resource_type);
  static ::util::Status VerifyName(const ::p4::config::v1::Preamble& preamble,
                                   const std::string& resource_type);

  // Stores a copy of the injected P4Info.
  const ::p4::config::v1::P4Info p4_info_;

  // One P4ResourceMap exists for every type of P4 resource that this
  // instance manages.
  P4ResourceMap<::p4::config::v1::Table> table_map_;
  P4ResourceMap<::p4::config::v1::Action> action_map_;
  P4ResourceMap<::p4::config::v1::ActionProfile> action_profile_map_;
  P4ResourceMap<::p4::config::v1::Counter> counter_map_;
  P4ResourceMap<::p4::config::v1::DirectCounter> direct_counter_map_;
  P4ResourceMap<::p4::config::v1::Meter> meter_map_;
  P4ResourceMap<::p4::config::v1::DirectMeter> direct_meter_map_;
  P4ResourceMap<::p4::config::v1::ValueSet> value_set_map_;
  P4ResourceMap<::p4::config::v1::Register> register_map_;
  P4ResourceMap<::p4::config::v1::Digest> digest_map_;

  // These containers verify that all P4 names and IDs are unique across all
  // types of resources that have an embedded Preamble.
  absl::flat_hash_set<uint32> all_resource_ids_;
  absl::flat_hash_map<std::string, const ::p4::config::v1::Preamble*>
      all_resource_names_;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_P4_P4_INFO_MANAGER_H_
