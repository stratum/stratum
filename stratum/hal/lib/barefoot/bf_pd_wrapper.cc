// Copyright 2020-present Open Networking Founcation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bf_pd_wrapper.h"

extern "C" {
#include "tofino/pdfixed/pd_devport_mgr.h"
#include "tofino/pdfixed/pd_tm.h"
}

#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace barefoot {

BFPdWrapper* BFPdWrapper::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex BFPdWrapper::init_lock_(absl::kConstInit);

// Gets the CPU port of an unit(device).
::util::StatusOr<int> BFPdWrapper::GetPcieCpuPort(int unit) {
  int port = p4_devport_mgr_pcie_cpu_port_get(unit);
  CHECK_RETURN_IF_FALSE(port != -1);
  return port;
}

// Sets the CPU port to the traffic manager.
::util::Status BFPdWrapper::SetTmCpuPort(int unit, int port) {
  CHECK_RETURN_IF_FALSE(p4_pd_tm_set_cpuport(unit, port) == 0)
      << "Unable to set CPU port " << port << " to unit " << unit;
  return ::util::OkStatus();
}

BFPdWrapper* BFPdWrapper::GetSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new BFPdWrapper();
  }

  return singleton_;
}

BFPdWrapper::BFPdWrapper() {}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
