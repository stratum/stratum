// Copyright 2019-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <memory>

#include "absl/synchronization/mutex.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bf_pal_interface.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/channel/channel.h"

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_PAL_WRAPPER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_PAL_WRAPPER_H_

namespace stratum {
namespace hal {
namespace barefoot {

class BFPalWrapper : public BFPalInterface {
 public:
  static constexpr int32 kDefaultMtu = 10 * 1024;  // 10K

  ::util::StatusOr<PortState> PortOperStateGet(int unit,
                                               uint32 port_id) override;

  ::util::Status PortAllStatsGet(int unit, uint32 port_id,
                                 PortCounters* counters) override;

  ::util::Status PortStatusChangeRegisterEventWriter(
      std::unique_ptr<ChannelWriter<PortStatusChangeEvent> > writer) override
      LOCKS_EXCLUDED(port_status_change_event_writer_lock_);

  ::util::Status PortStatusChangeUnregisterEventWriter() override
      LOCKS_EXCLUDED(port_status_change_event_writer_lock_);

  ::util::Status PortAdd(int unit, uint32 port_id, uint64 speed_bps,
                         FecMode fec_mode) override;

  ::util::Status PortDelete(int unit, uint32 port_id) override;

  ::util::Status PortEnable(int unit, uint32 port_id) override;

  ::util::Status PortDisable(int unit, uint32 port_id) override;

  ::util::Status PortAutonegPolicySet(int unit, uint32 port_id,
                                      TriState autoneg) override;

  ::util::Status PortMtuSet(int unit, uint32 port_id, int32 mtu) override;

  bool PortIsValid(int unit, uint32 port_id) override;

  ::util::Status PortLoopbackModeSet(int uint, uint32 port_id,
                                     LoopbackState loopback_mode) override;

  static BFPalWrapper* GetSingleton();

  // BFPalWrapper is neither copyable nor movable.
  BFPalWrapper(const BFPalWrapper&) = delete;
  BFPalWrapper& operator=(const BFPalWrapper&) = delete;
  BFPalWrapper(BFPalWrapper&&) = delete;
  BFPalWrapper& operator=(BFPalWrapper&&) = delete;

 private:
  // Private constructor, use GetSingleton()
  BFPalWrapper();

  friend ::util::Status PortStatusChangeCb(int unit, uint32 port_id, bool up,
                                           void* cookie)
      LOCKS_EXCLUDED(port_status_change_event_writer_lock_);

  std::unique_ptr<ChannelWriter<PortStatusChangeEvent> >
      port_status_change_event_writer_
          GUARDED_BY(port_status_change_event_writer_lock_);

  mutable absl::Mutex port_status_change_event_writer_lock_;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_PAL_WRAPPER_H_
