// Copyright 2022-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_P4RUNTIME_TRANSLATOR_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_P4RUNTIME_TRANSLATOR_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bf_sde_interface.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtP4RuntimeTranslator {
 public:
  virtual ~BfrtP4RuntimeTranslator() = default;
  virtual ::util::Status PushChassisConfig(const ChassisConfig& config,
                                           uint64 node_id)
      LOCKS_EXCLUDED(lock_);
  virtual ::util::Status PushForwardingPipelineConfig(
      const ::p4::config::v1::P4Info& p4info) LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::v1::Entity> TranslateEntity(
      const ::p4::v1::Entity& entity, bool to_sdk) LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::v1::ReadRequest> TranslateReadRequest(
      const ::p4::v1::ReadRequest& request) LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::v1::ReadResponse> TranslateReadResponse(
      const ::p4::v1::ReadResponse& request) LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::v1::StreamMessageRequest>
  TranslateStreamMessageRequest(const ::p4::v1::StreamMessageRequest& request)
      LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::v1::StreamMessageResponse>
  TranslateStreamMessageResponse(
      const ::p4::v1::StreamMessageResponse& response) LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::config::v1::P4Info> GetLowLevelP4Info()
      LOCKS_EXCLUDED(lock_);
  static std::unique_ptr<BfrtP4RuntimeTranslator> CreateInstance(
      BfSdeInterface* bf_sde_interface, int device_id,
      bool translation_enabled) {
    return absl::WrapUnique(new BfrtP4RuntimeTranslator(
        bf_sde_interface, device_id, translation_enabled));
  }

 protected:
  // Default constructor.
  BfrtP4RuntimeTranslator()
      : device_id_(0),
        bf_sde_interface_(nullptr),
        translation_enabled_(false),
        pipeline_require_translation_(false) {}

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BfrtP4RuntimeTranslator(BfSdeInterface* bf_sde_interface, int device_id,
                          bool translation_enabled)
      : device_id_(device_id),
        bf_sde_interface_(bf_sde_interface),
        translation_enabled_(translation_enabled),
        pipeline_require_translation_(false) {}
  virtual ::util::StatusOr<::p4::v1::Entity> TranslateEntityInternal(
      const ::p4::v1::Entity& entity, bool to_sdk) SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::TableEntry> TranslateTableEntry(
      const ::p4::v1::TableEntry& entry, bool to_sdk)
      SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::ActionProfileMember>
  TranslateActionProfileMember(const ::p4::v1::ActionProfileMember& entry,
                               bool to_sdk) SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::MeterEntry> TranslateMeterEntry(
      const ::p4::v1::MeterEntry& entry, bool to_sdk)
      SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::DirectMeterEntry>
  TranslateDirectMeterEntry(const ::p4::v1::DirectMeterEntry& entry,
                            bool to_sdk) SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::CounterEntry> TranslateCounterEntry(
      const ::p4::v1::CounterEntry& entry, bool to_sdk)
      SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::DirectCounterEntry>
  TranslateDirectCounterEntry(const ::p4::v1::DirectCounterEntry& entry,
                              bool to_sdk) SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::RegisterEntry> TranslateRegisterEntry(
      const ::p4::v1::RegisterEntry& entry, bool to_sdk)
      SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>
  TranslatePacketReplicationEngineEntry(
      const ::p4::v1::PacketReplicationEngineEntry& entry, bool to_sdk)
      SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::PacketMetadata> TranslatePacketMetadata(
      const p4::v1::PacketMetadata& packet_metadata, const std::string& uri,
      int32 bit_width, bool to_sdk) SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::PacketIn> TranslatePacketIn(
      const ::p4::v1::PacketIn& packet_in) SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::PacketOut> TranslatePacketOut(
      const ::p4::v1::PacketOut& packet_out) SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::Replica> TranslateReplica(
      const ::p4::v1::Replica& replica, bool to_sdk)
      SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::Action> TranslateAction(
      const ::p4::v1::Action& action, bool to_sdk) SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<::p4::v1::Index> TranslateIndex(
      const ::p4::v1::Index& index, const std::string& uri, bool to_sdk)
      SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<std::string> TranslateValue(const std::string& value,
                                                       const std::string& uri,
                                                       bool to_sdk,
                                                       int32 bit_width)
      SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<std::string> TranslateTnaPortId(
      const std::string& value, bool to_sdk, int32 bit_width)
      SHARED_LOCKS_REQUIRED(lock_);

  // Reader-writer lock used to protect access to specific states.
  mutable absl::Mutex lock_;

  // Fixed zero-based BFRT device_id number corresponding to the node/ASIC
  // managed by this class instance. Assigned in the class constructor.
  const int device_id_;

  // Pointer to a BfSdeInterface implementation that wraps all the SDE calls.
  // Not owned by this class.
  BfSdeInterface* bf_sde_interface_ = nullptr;

  // Maps between singleton port and SDK port, vice versa
  absl::flat_hash_map<uint32, uint32> singleton_port_to_sdk_port_
      GUARDED_BY(lock_);
  absl::flat_hash_map<uint32, uint32> sdk_port_to_singleton_port_
      GUARDED_BY(lock_);
  const bool translation_enabled_;
  bool pipeline_require_translation_ GUARDED_BY(lock_);

  // P4Runtime translation information
  absl::flat_hash_map<uint32, absl::flat_hash_map<uint32, std::string>>
      table_to_field_to_type_uri_ GUARDED_BY(lock_);
  absl::flat_hash_map<uint32, absl::flat_hash_map<uint32, std::string>>
      action_to_param_to_type_uri_ GUARDED_BY(lock_);
  absl::flat_hash_map<uint32, std::string> packet_in_meta_to_type_uri_
      GUARDED_BY(lock_);
  absl::flat_hash_map<uint32, std::string> packet_out_meta_to_type_uri_
      GUARDED_BY(lock_);
  absl::flat_hash_map<uint32, std::string> counter_to_type_uri_
      GUARDED_BY(lock_);
  absl::flat_hash_map<uint32, std::string> meter_to_type_uri_ GUARDED_BY(lock_);
  absl::flat_hash_map<uint32, std::string> register_to_type_uri_
      GUARDED_BY(lock_);
  absl::flat_hash_map<uint32, absl::flat_hash_map<uint32, int32>>
      table_to_field_to_bit_width_ GUARDED_BY(lock_);
  absl::flat_hash_map<uint32, absl::flat_hash_map<uint32, int32>>
      action_to_param_to_bit_width_ GUARDED_BY(lock_);
  absl::flat_hash_map<uint32, int32> packet_in_meta_to_bit_width_
      GUARDED_BY(lock_);
  absl::flat_hash_map<uint32, int32> packet_out_meta_to_bit_width_
      GUARDED_BY(lock_);

  // This P4Info contains field information without P4Runtime translation
  // which is useful for low level managers.
  ::p4::config::v1::P4Info low_level_p4info_;

  static constexpr uint32 kSdnTnaRecirculationPortBase = 0xFFFFFF00;
  static constexpr uint32 kTnaRecirculationPortBase = 0x44;
  static constexpr int32 kTnaMaxNumPipes = 4;
  friend class BfrtP4RuntimeTranslatorTest;

 public:
  // Wrapper for writers
  class ReadResponseWriterWrapper
      : public WriterInterface<::p4::v1::ReadResponse> {
   public:
    ReadResponseWriterWrapper(
        WriterInterface<::p4::v1::ReadResponse>* writer,
        BfrtP4RuntimeTranslator* bfrt_p4runtime_translator)
        : writer_(ABSL_DIE_IF_NULL(writer)),
          bfrt_p4runtime_translator_(
              ABSL_DIE_IF_NULL(bfrt_p4runtime_translator)) {}
    bool Write(const ::p4::v1::ReadResponse& msg) override;

   private:
    // The original writer, not owned by this class.
    WriterInterface<::p4::v1::ReadResponse>* writer_;
    // The pointer point to the translator, not owned by this class.
    BfrtP4RuntimeTranslator* bfrt_p4runtime_translator_;

    friend class TranslatorWriterWrapperTest;
  };

  class StreamMessageResponseWriterWrapper
      : public WriterInterface<::p4::v1::StreamMessageResponse> {
   public:
    StreamMessageResponseWriterWrapper(
        std::shared_ptr<WriterInterface<::p4::v1::StreamMessageResponse>>
            writer,
        BfrtP4RuntimeTranslator* bfrt_p4runtime_translator)
        : writer_(writer),
          bfrt_p4runtime_translator_(
              ABSL_DIE_IF_NULL(bfrt_p4runtime_translator)) {}
    bool Write(const ::p4::v1::StreamMessageResponse& msg) override;

   private:
    // The original writer, not owned by this class.
    std::shared_ptr<WriterInterface<::p4::v1::StreamMessageResponse>> writer_;
    // The pointer point to the translator, not owned by this class.
    BfrtP4RuntimeTranslator* bfrt_p4runtime_translator_;

    friend class TranslatorWriterWrapperTest;
  };
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_P4RUNTIME_TRANSLATOR_H_
