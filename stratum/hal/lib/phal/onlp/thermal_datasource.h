/*
 * Copyright 2019 Edgecore Networks Corporation
 * Phani Karanam <phani_karanam@edge-core.com>
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_THERMAL_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_THERMAL_DATASOURCE_H_

#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
// FIXME remove when onlp_wrapper.h is stable
// #include "stratum/hal/lib/phal/onlp/onlp_wrapper_fake.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/system_interface.h"
#include "stratum/lib/macros.h"
#include "stratum/glue/integral_types.h"
#include "absl/memory/memory.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class OnlpThermalDataSource : public DataSource {
  // Makes a shared_ptr to an ThermalDataSource which manages an ONLP THERMAL object.
  // Returns error if the OID object is not of the correct type or not present.
 public:
  // OnlpThermalDataSource does not take ownership of onlp_interface. We expect
  // onlp_interface remains valid during OnlpThermalDataSource's lifetime.
  static ::util::StatusOr<std::shared_ptr<OnlpThermalDataSource>> Make(
      OnlpOid thermal_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy);
      
  ::util::Status Iscapable(ThermalCaps thermal_caps);

  // Accessors for managed attributes.
  ManagedAttribute* GetThermalId() { return &thermal_id_; }
  ManagedAttribute* GetThermalHardwareState() { return &thermal_hw_state_; }
  ManagedAttribute* GetThermalCurTemp() { return &thermal_cur_temp_; }
  ManagedAttribute* GetThermalWarnTemp() { return &thermal_warn_temp_; }
  ManagedAttribute* GetThermalErrorTemp() { return &thermal_error_temp_; }
  ManagedAttribute* GetThermalShutDownTemp() { return &thermal_shut_down_temp_; }

 private:
  OnlpThermalDataSource(OnlpOid thermal_id, OnlpInterface* onlp_interface,
                    CachePolicy* cache_policy, const ThermalInfo& thermal_info);

  static ::util::Status ValidateOnlpThermalInfo(OnlpOid oid,
                                            OnlpInterface* onlp_interface) {
    ASSIGN_OR_RETURN(OidInfo oid_info, onlp_interface->GetOidInfo(oid));
    CHECK_RETURN_IF_FALSE(oid_info.Present())
        << "The THERMAL with OID " << oid << " is not currently present.";
    return ::util::OkStatus();
  }

  ::util::Status UpdateValues() override;
  OnlpOid thermal_oid_;

  // We do not own ONLP stub object. ONLP stub is created on PHAL creation and
  // destroyed when PHAL deconstruct. Do not delete onlp_stub_.
  OnlpInterface* onlp_stub_;

  // A list of managed attributes.
  // Hardware Info.
  TypedAttribute<OnlpOid> thermal_id_{this};
  EnumAttribute thermal_hw_state_{HwState_descriptor(), this};
  TypedAttribute<double> thermal_cur_temp_{this};
  TypedAttribute<double> thermal_warn_temp_{this};
  TypedAttribute<double> thermal_error_temp_{this};
  TypedAttribute<double> thermal_shut_down_temp_{this};
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_THERMAL_DATASOURCE_H_
