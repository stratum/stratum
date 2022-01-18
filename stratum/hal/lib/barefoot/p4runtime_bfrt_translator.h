// Copyright 2022-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_P4RUNTIME_BFRT_TRANSLATOR_H_
#define STRATUM_HAL_LIB_BAREFOOT_P4RUNTIME_BFRT_TRANSLATOR_H_

#include <map>
#include <memory>
#include <string>

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

class P4RuntimeBfrtTranslator {
 public:
  virtual ~P4RuntimeBfrtTranslator() = default;
  virtual ::util::Status PushChassisConfig(const ChassisConfig& config)
      LOCKS_EXCLUDED(lock_);
  virtual ::util::Status PushForwardingPipelineConfig(
      const ::p4::config::v1::P4Info& p4info) LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::v1::TableEntry> TranslateTableEntry(
      const ::p4::v1::TableEntry& entry, const bool& to_sdk)
      LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::v1::ActionProfileMember>
  TranslateActionProfileMember(const ::p4::v1::ActionProfileMember& entry,
                               const bool& to_sdk) LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::v1::MeterEntry> TranslateMeterEntry(
      const ::p4::v1::MeterEntry& entry, const bool& to_sdk)
      LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::v1::DirectMeterEntry>
  TranslateDirectMeterEntry(const ::p4::v1::DirectMeterEntry& entry,
                            const bool& to_sdk) LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::v1::CounterEntry> TranslateCounterEntry(
      const ::p4::v1::CounterEntry& entry, const bool& to_sdk)
      LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::v1::DirectCounterEntry>
  TranslateDirectCounterEntry(const ::p4::v1::DirectCounterEntry& entry,
                              const bool& to_sdk) LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::v1::RegisterEntry> TranslateRegisterEntry(
      const ::p4::v1::RegisterEntry& entry, const bool& to_sdk)
      LOCKS_EXCLUDED(lock_);
  virtual ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>
  TranslatePacketReplicationEngineEntry(
      const ::p4::v1::PacketReplicationEngineEntry& entry, const bool& to_sdk)
      LOCKS_EXCLUDED(lock_);
  static std::unique_ptr<P4RuntimeBfrtTranslator> CreateInstance(
      BfSdeInterface* bf_sde_interface, int device_id) {
    return absl::WrapUnique(
        new P4RuntimeBfrtTranslator(bf_sde_interface, device_id));
  }
  virtual ::util::StatusOr<std::string> TranslateValue(const std::string& value,
                                                       const std::string& uri,
                                                       const bool& to_sdk,
                                                       const int32& bit_width)
      SHARED_LOCKS_REQUIRED(lock_);
  virtual ::util::StatusOr<std::string> TranslateTnaPortId(
      const std::string& value, const bool& to_sdk, const int32& bit_width)
      SHARED_LOCKS_REQUIRED(lock_);
  virtual bool TranslationEnabled() LOCKS_EXCLUDED(lock_);

 protected:
  // Default constructor.
  P4RuntimeBfrtTranslator()
      : device_id_(0),
        bf_sde_interface_(nullptr),
        translation_enabled_(false) {}

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  P4RuntimeBfrtTranslator(BfSdeInterface* bf_sde_interface, int device_id)
      : device_id_(device_id),
        bf_sde_interface_(bf_sde_interface),
        translation_enabled_(false) {}

  // Reader-writer lock used to protect access to specific states.
  mutable absl::Mutex lock_;

  // Fixed zero-based BFRT device_id number corresponding to the node/ASIC
  // managed by this class instance. Assigned in the class constructor.
  const int device_id_;

  // Pointer to a BfSdeInterface implementation that wraps all the SDE calls.
  // Not owned by this class.
  BfSdeInterface* bf_sde_interface_ = nullptr;

  // Maps between singleton port and SDK port, vice versa
  std::map<uint32, uint32> singleton_port_to_sdk_port_ GUARDED_BY(lock_);
  std::map<uint32, uint32> sdk_port_to_singleton_port_ GUARDED_BY(lock_);
  bool translation_enabled_ GUARDED_BY(lock_);

  // P4Runtime translation information
  std::map<uint32, std::map<uint32, std::string>> table_to_field_to_type_uri_
      GUARDED_BY(lock_);
  std::map<uint32, std::map<uint32, std::string>> action_to_param_to_type_uri_
      GUARDED_BY(lock_);
  std::map<uint32, std::map<uint32, std::string>> ctrl_hdr_to_meta_to_type_uri_
      GUARDED_BY(lock_);
  std::map<uint32, std::string> counter_to_type_uri_ GUARDED_BY(lock_);
  std::map<uint32, std::string> meter_to_type_uri_ GUARDED_BY(lock_);
  std::map<uint32, std::string> register_to_type_uri_ GUARDED_BY(lock_);

  std::map<uint32, std::map<uint32, int32>> table_to_field_to_bit_width_
      GUARDED_BY(lock_);
  std::map<uint32, std::map<uint32, int32>> action_to_param_to_bit_width_
      GUARDED_BY(lock_);
  std::map<uint32, std::map<uint32, int32>> ctrl_hdr_to_meta_to_bit_width_
      GUARDED_BY(lock_);
  std::map<uint32, int32> counter_to_bit_width_ GUARDED_BY(lock_);
  std::map<uint32, int32> meter_to_bit_width_ GUARDED_BY(lock_);
  std::map<uint32, int32> register_to_bit_width_ GUARDED_BY(lock_);

  friend class P4RuntimeBfrtTranslatorTest;
};

class P4RuntimeBfrtTranslationWriterWrapper
    : public WriterInterface<::p4::v1::ReadResponse> {
 public:
  P4RuntimeBfrtTranslationWriterWrapper(
      WriterInterface<::p4::v1::ReadResponse>* writer,
      P4RuntimeBfrtTranslator* p4runtime_bfrt_translator)
      : writer_(ABSL_DIE_IF_NULL(writer)),
        p4runtime_bfrt_translator_(
            ABSL_DIE_IF_NULL(p4runtime_bfrt_translator)) {}
  bool Write(const ::p4::v1::ReadResponse& msg) override;

 private:
  // The original writer, not owned by this class.
  WriterInterface<::p4::v1::ReadResponse>* writer_;
  // The pointer point to the translator, not owned by this class.
  P4RuntimeBfrtTranslator* p4runtime_bfrt_translator_;

  friend class TranslatorWriterWrapperTest;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_P4RUNTIME_BFRT_TRANSLATOR_H_
