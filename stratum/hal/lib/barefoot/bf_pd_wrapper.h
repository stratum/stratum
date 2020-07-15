// Copyright 2020-present Open Networking Founcation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_PD_WRAPPER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_PD_WRAPPER_H_

#include "absl/synchronization/mutex.h"
#include "stratum/hal/lib/barefoot/bf_pd_interface.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BFPdWrapper : public BFPdInterface {
 public:
  // Gets the CPU port of an unit(device).
  ::util::StatusOr<int> GetPcieCpuPort(int unit) override;

  // Sets the CPU port to the traffic manager.
  ::util::Status SetTmCpuPort(int unit, int port) override;

  static BFPdWrapper* GetSingleton() LOCKS_EXCLUDED(init_lock_);

  // BFPdWrapper is neither copyable nor movable.
  BFPdWrapper(const BFPdWrapper&) = delete;
  BFPdWrapper& operator=(const BFPdWrapper&) = delete;
  BFPdWrapper(BFPdWrapper&&) = delete;
  BFPdWrapper& operator=(BFPdWrapper&&) = delete;

 private:
  BFPdWrapper();  // Use GetSingleton

  // RW mutex lock for protecting the singleton instance initialization and
  // reading it back from other threads. Unlike other singleton classes, we
  // use RW lock as we need the pointer to class to be returned.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static BFPdWrapper* singleton_ GUARDED_BY(init_lock_);
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_PD_WRAPPER_H_
