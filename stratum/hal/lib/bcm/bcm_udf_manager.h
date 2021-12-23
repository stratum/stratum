// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BCM_BCM_UDF_MANAGER_H_
#define STRATUM_HAL_LIB_BCM_BCM_UDF_MANAGER_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "stratum/hal/lib/bcm/acl_table.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/bcm/bcm_sdk_interface.h"
#include "stratum/hal/lib/p4/p4_table_mapper.h"

namespace stratum {

namespace hal {
namespace bcm {

// BcmUdfManager manages all the UDF sets in a single node. This class is
// responsible for converting match fields to UDF, accounting UDF fields, and
// setting up the match fields in hardware. This class is expected to be used
// directly by BcmAclManager.
//
// After construction, SetUpStaticUdfs() should be called with all the ACL
// tables to set up any UDF banks used for switch-defined UDF conversions.
//
// TODO(richardyu): UDF setup for tables with controller-defined UDFs is not yet
// implemented.
//
// After the UDF manager has been set up, MappedFieldToBcmFields() will return a
// BcmFlow using the UDF conversion for a match field.
class BcmUdfManager {
 public:
  // ***************************************************************************
  // Factory Functions
  // ***************************************************************************
  // Creates and returns a unique pointer to a BcmUdfManager. Performs some
  // sanity checking on the input.
  // Arguments:
  //     bcm_sdk_interface: Interface for accessing the Broadcom SDK.
  //                        BcmUdfManager does not take ownership of the object,
  //                        which must outlive the BcmUdfManager.
  //              udf_spec: The hardware UDF specification for this chip.
  //   num_controller_sets: The number of UDF sets reserved for controller use.
  //                  unit: Zero-based BCM unit number corresponding to the
  //                        node/ASIC managed by this instance.
  //       p4_table_mapper: A pointer to a P4TableMapper object. BcmUdfManager
  //                        does not take ownership of the object, which must
  //                        outlive the BcmUdfManager.
  //       is_udf_eligible: A function that returns true if a mapped field
  //                        should be treated as a static UDF.
  // Creates a BcmUdfManager using UDf information from the UDF spec.
  static ::util::StatusOr<std::unique_ptr<BcmUdfManager>> CreateInstance(
      BcmSdkInterface* bcm_sdk_interface,
      const BcmHardwareSpecs::ChipModelSpec::UdfSpec& udf_spec,
      int num_controller_sets, int unit, const P4TableMapper* p4_table_mapper,
      std::function<bool(const MappedField&, BcmAclStage)> is_udf_eligible =
          DefaultIsUdfEligible);

  // ***************************************************************************
  // Initializers
  // ***************************************************************************
  // Sets up the static UDF set(s) to accommodate a set of ACL tables.
  // Updates the ACL tables with the UDF fields and associated UDF set. This
  // should be called after the ACL tables are generated from the P4 config but
  // before the ACL tables are installed into hardware.
  ::util::Status SetUpStaticUdfs(std::vector<AclTable>* acl_tables);

  // Installs all of the known UDF chunks into hardware. This should be called
  // after SetUpStaticUdfs() and after every dynamic UDF setup.
  ::util::Status InstallUdfs();

  // ***************************************************************************
  // Member Functions
  // ***************************************************************************
  // Converts a mapped field to BcmFields that use UDFs. Returns an error if
  // the field cannot be implemented using the UDFs managed by this
  // BcmUdfManager.
  ::util::StatusOr<std::vector<BcmField>> MappedFieldToBcmFields(
      int udf_set_id, const MappedField& mapped_field);

  // TODO(richardyu): Implement the dynamic UDF logic once we know how that will
  // look.

 protected:
  // ***************************************************************************
  // Constructor
  // ***************************************************************************
  // Constructs a BcmUdfManager object. Prefer to use factory methods for actual
  // construction.
  //
  // Arguments:
  //     bcm_sdk_interface: An accessor class for the Broadcom SDK. Not owned by
  //                        this class.
  //              udf_spec: The hardware UDF specification for this chip.
  //   num_controller_sets: The number of UDF sets reserved for controller use.
  //                  unit: Zero-based BCM unit number corresponding to the
  //                        node/ASIC managed by this instance.
  //       p4_table_mapper: A pointer to a P4TableMapper object. BcmUdfManager
  //                        does not take overship of the object, which must
  //                        outlive the BcmUdfManager.
  //       is_udf_eligible: A function that returns true if a mapped field
  //                        should be treated as a static UDF.
  BcmUdfManager(
      BcmSdkInterface* bcm_sdk_interface,
      const BcmHardwareSpecs::ChipModelSpec::UdfSpec& udf_spec,
      int num_controller_sets, int unit, const P4TableMapper* p4_table_mapper,
      std::function<bool(const MappedField&, BcmAclStage)> is_udf_eligible);

  // This constructor is the same as above, but it uses the default UDF
  // eligibility classifier.
  BcmUdfManager(BcmSdkInterface* bcm_sdk_interface,
                const BcmHardwareSpecs::ChipModelSpec::UdfSpec& udf_spec,
                int num_controller_sets, int unit,
                const P4TableMapper* p4_table_mapper)
      : BcmUdfManager(bcm_sdk_interface, udf_spec, num_controller_sets, unit,
                      p4_table_mapper, DefaultIsUdfEligible) {}

 private:
  // ***************************************************************************
  // Data Types
  // ***************************************************************************

  // Usage types allowed for each UDF set.
  enum UdfSetUsage {
    kController,  // UDF chunks are defined by the controller during runtime.
    kStatic,      // UDF chunks are defined by the switchstack during config.
  };

  // This class represents a single UDF chunk. UDF chunks are defined by:
  // * A packet layer
  // * An offset from the start of the packet layer in bits. Must be a multiple
  //   of the chunk size.
  // * A chunk size (in bits).
  // * A unique chunk id
  class UdfChunk {
   public:
    // Default constructor to create invalid chunks.
    UdfChunk()
        : packet_layer_(BcmUdfSet::UNKNOWN), offset_(0), size_(0), id_(0) {}

    // Normal constructors.
    UdfChunk(BcmUdfSet::PacketLayer packet_layer, int offset, int size, int id)
        : packet_layer_(packet_layer),
          offset_(offset - (offset % size)),
          size_(size),
          id_(id) {}
    UdfChunk(BcmUdfSet::PacketLayer packet_layer, int offset, int size)
        : UdfChunk(packet_layer, offset, size, 0) {}

    ~UdfChunk() {}

    // Static functions.
    // Returns a set of UdfChunks required to implement a MappedField qualifier.
    static std::vector<UdfChunk> MappedFieldToUdfs(
        const MappedField& mapped_field, int chunk_size);

    // Initializers.
    // Update the UDF chunk ID.
    void set_id(int id) { id_ = id; }

    // Accessors.
    // Returns the ID of this chunk.
    int id() const { return id_; }

    // Returns the packet layer for this chunk.
    BcmUdfSet::PacketLayer packet_layer() const { return packet_layer_; }

    // Returns the offset for this chunk in bits.
    int bit_offset() const { return offset_; }

    // Returns the offset for this chunk in bytes.
    int byte_offset() const { return offset_ / 8; }

    // Returns the size of this chunk in bits.
    int bit_size() const { return size_; }

    // Returns the size of this chunk in bytes.
    int byte_size() const { return (size_ + 7) / 8; }

    // Returns a string representation of this UDF Chunk.
    std::string ToString() const {
      return absl::StrCat("(layer: ", packet_layer(),
                          ", offset: ", bit_offset(), " bits",
                          ", size: ", bit_size(), " bits)");
    }

   private:
    BcmUdfSet::PacketLayer packet_layer_;  // Packet layer for this chunk.
    int offset_;  // Byte offset of the chunk within the packet layer.
    int size_;    // Size of the chunk in bits.
    int id_;      // Unique ID for this UDF chunk.
  };

  // Hash function for UdfChunk objects.
  struct UdfChunkHash {
    size_t operator()(const UdfChunk& chunk) const {
      // 4096 is much greater than the maximum practical value of offset_
      // (256 * 8 = 2048).
      return chunk.packet_layer() * 4096 + chunk.bit_offset();
    }
  };

  // Hash comparison function for UdfChunk objects.
  struct UdfChunkEq {
    bool operator()(const UdfChunk& x, const UdfChunk& y) const {
      return x.packet_layer() == y.packet_layer() &&
             x.bit_offset() == y.bit_offset();
    }
  };

  // A UDF set is a collection of UDF chunks. An ACL table may only access UDFs
  // from a single set. Each UdfSet must contain all UDFs needed by any table
  // that references it.
  class UdfSet {
   public:
    // Constructor.
    // usage: The usage type for this UDF Set.
    // base_chunk_id: The first ID to assign to chunks managed by this UDF set.
    //                UDF chunk IDs are allocated in the range of:
    //                [base_chunk_id, base_chunk_id + max_chunks - 1].
    // max_chunks: The maximum number of chunks the set can hold.
    UdfSet(UdfSetUsage usage, int base_chunk_id, int max_chunks)
        : chunks_(),
          usage_(usage),
          base_chunk_id_(base_chunk_id),
          max_chunks_(max_chunks) {}

    // Lazy constructor. Used for temporary (non-hardware) UDF sets.
    explicit UdfSet(int max_chunks) : UdfSet(kStatic, 0, max_chunks) {}

    // Empty constructor for ::util::StatusOr<BcmUdfManager::UdfSet> uses
    UdfSet() : UdfSet(kStatic, 0, 0) {}

    // Returns read-only view of the chunks in this set.
    const absl::flat_hash_set<UdfChunk, UdfChunkHash, UdfChunkEq>& chunks()
        const {
      return chunks_;
    }

    // Returns the usage type for this set.
    UdfSetUsage usage() const { return usage_; }

    // Adds a collection of UdfChunk objects into this table.
    // <T> must be an iterable collection of UdfChunk objects.
    template <typename T>
    ABSL_MUST_USE_RESULT bool AddChunks(const T& chunks);

    // Merges another UDF set into this one. Returns false if the resulting set
    // is too large. Replaces all the chunk IDs from the merged set based on the
    // available range for this UdfSet.
    ABSL_MUST_USE_RESULT bool MergeFrom(UdfSet other) {
      return AddChunks(other.chunks_);
    }

   private:
    absl::flat_hash_set<UdfChunk, UdfChunkHash, UdfChunkEq>
        chunks_;         // The colleciton of UDF chunks managed by this set.
    UdfSetUsage usage_;  // The usage type for this UDF set.
    int base_chunk_id_;  // The ID of the first chunk in this set.
    int max_chunks_;     // The maximum number of chunks in this set.
  };

  // ***************************************************************************
  // Static Functions
  // ***************************************************************************
  // Returns true if a field should be treated as a UDF field.
  // This is the default function for determining UDF eligibility. Used when
  // is_udf_eligible_ is not set during construction.
  static bool DefaultIsUdfEligible(const MappedField& mapped_field,
                                   BcmAclStage stage);

  // Creates a BcmField object that fills in the provided UDF chunk with the
  // overlapping sections of the MappedField. Any non-overlapping bits are set
  // to 0. Any non-relevant bits in mapped_field value/mask buffers must be set
  // to 0.
  static ::util::StatusOr<BcmField> CreateBcmField(
      const UdfChunk& chunk, const MappedField& mapped_field);

  // ***************************************************************************
  // Member Functions
  // ***************************************************************************
  // Returns a set containing the static UDF chunks that apply to a given
  // AclTable. Only the chunks_ field of the UdfSet will be filled out.
  // If udf_match_fields is not null, the vector will be populated with the
  // list of UDF match fields.
  ::util::StatusOr<UdfSet> StaticUdfSetFromAclTable(
      const AclTable& table, std::vector<uint32>* udf_match_fields);

  // Allocates a UDF set to the least impactful destination set. Impact is
  // calculated as the increase in size to the destination set from adding the
  // input set.
  ::util::StatusOr<int> AllocateUdfSet(
      const UdfSet& input_set, const std::vector<int>& destination_sets);

  // Returns a subset of udf_sets_ keys containing all UdfSets of a given usage
  // type.
  std::vector<int> UdfSetsByUsage(UdfSetUsage usage) {
    std::vector<int> udf_sets;
    for (auto& pair : udf_sets_) {
      if (pair.second.usage() == usage) {
        udf_sets.push_back(pair.first);
      }
    }
    return udf_sets;
  }

  // ***************************************************************************
  // Member Variables
  // ***************************************************************************
  BcmSdkInterface* bcm_sdk_interface_;  // Interface to the Bcm SDK. Not owned
                                        // by this class.
  std::map<int, UdfSet> udf_sets_;      // UDF sets managed by this object.
  int chunk_size_;                      // The size of each chunk in bits.
  int chunks_per_set_;                  // Number of chunks available per set.

  // Fixed zero-based BCM unit number corresponding to the node/ASIC managed by
  // this class instance. Assigned in the class constructor.
  const int unit_;

  const P4TableMapper* p4_table_mapper_;  // Used to lookup p4 field types. Not
                                          // owned by this object.
  std::function<bool(const MappedField&, BcmAclStage)>
      is_udf_eligible_;  // UDF-Eligibility lookup function.
};

}  // namespace bcm
}  // namespace hal

}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_UDF_MANAGER_H_
