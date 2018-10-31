/*
 * Copyright 2018 Google LLC
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

#ifndef THIRD_PARTY_STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_H_
#define THIRD_PARTY_STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_H_

extern "C" {
#include "sandblaze/onlp/include/onlp/oids.h"
#include "sandblaze/onlp/include/onlp/sfp.h"
}
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/macros.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/status/status.h"
#include "util/task/statusor.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using OnlpOid = onlp_oid_t;
using OnlpOidHeader = onlp_oid_hdr_t;
using SffDomInfo = sff_dom_info_t;
using SffInfo = sff_info_t;

// This class encapsulates information that exists for every type of OID. More
// specialized classes for specific OID types should derive from this.
class OidInfo {
 public:
  explicit OidInfo(const onlp_oid_hdr_t& oid_info) : oid_info_(oid_info) {}

  HwState GetHardwareState() const;
  bool Present() const;
  const OnlpOidHeader* GetHeader() const { return &oid_info_; }

 private:
  onlp_oid_hdr_t oid_info_;
};

class SfpInfo : public OidInfo {
 public:
  explicit SfpInfo(const onlp_sfp_info_t& sfp_info)
      : OidInfo(sfp_info.hdr), sfp_info_(sfp_info) {}

  MediaType GetMediaType() const;

  // The lifetimes of pointers returned by these functions are managed by this
  // object. The returned pointer will never be nullptr.
  const SffDomInfo* GetSffDomInfo() const { return &sfp_info_.dom; }
  ::util::StatusOr<const SffInfo*> GetSffInfo() const;

 private:
  onlp_sfp_info_t sfp_info_;
};

// A interface for ONLP calls.
// This class wraps c-style direct ONLP calls with Google-style c++ calls that
// return ::util::Status.
class OnlpInterface {
 public:
  virtual ~OnlpInterface() {}

  // Given a OID object id, returns SFP info or failure.
  virtual ::util::StatusOr<SfpInfo> GetSfpInfo(OnlpOid oid) const = 0;

  // Given an OID, returns the OidInfo for that object (or an error if it
  // doesn't exist
  virtual ::util::StatusOr<OidInfo> GetOidInfo(OnlpOid oid) const = 0;
};

// An OnlpInterface implementation that makes real calls into ONLP.
// Note that this wrapper performs ONLP setup and teardown, so only one may be
// allocated at any given time.
class OnlpWrapper : public OnlpInterface {
 public:
  static ::util::StatusOr<std::unique_ptr<OnlpWrapper>> Make();
  OnlpWrapper(const OnlpWrapper& other) = delete;
  OnlpWrapper& operator=(const OnlpWrapper& other) = delete;
  ~OnlpWrapper() override;

  ::util::StatusOr<OidInfo> GetOidInfo(OnlpOid oid) const override;
  ::util::StatusOr<SfpInfo> GetSfpInfo(OnlpOid oid) const override;

 private:
  OnlpWrapper() {}
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_HAL_LIB_PHAL_ONLP_ONLP_WRAPPER_H_
