// Copyright 2019 Edgecore Networks Corporation
//Phani Karanam <phani_karanam@edge-core.com>
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stratum/hal/lib/phal/onlp/thermal_datasource.h"

#include <cmath>
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/datasource.h"
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


::util::StatusOr<std::shared_ptr<OnlpThermalDataSource>> OnlpThermalDataSource::Make(
    OnlpOid thermal_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy) {
  RETURN_IF_ERROR_WITH_APPEND(ValidateOnlpThermalInfo(thermal_id, onlp_interface))
      << "Failed to create THERMAL datasource for OID: " << thermal_id;
  ASSIGN_OR_RETURN(ThermalInfo thermal_info, onlp_interface->GetThermalInfo(thermal_id));
  std::shared_ptr<OnlpThermalDataSource> thermal_data_source(
      new OnlpThermalDataSource(thermal_id, onlp_interface, cache_policy, thermal_info));

  // Retrieve attributes' initial values.
  // TODO: Move the logic to Configurator later?
  // thermal_data_source->updateValues();
  thermal_data_source->UpdateValuesUnsafelyWithoutCacheOrLock();
  return thermal_data_source;
}

OnlpThermalDataSource::OnlpThermalDataSource(OnlpOid thermal_id,
                                     OnlpInterface* onlp_interface,
                                     CachePolicy* cache_policy,
                                     const ThermalInfo& thermal_info)
    : DataSource(cache_policy), thermal_oid_(thermal_id), onlp_stub_(onlp_interface) {
  // Once the thermal present, the oid won't change. Do not add setter for id.
  thermal_id_.AssignValue(thermal_id);
}

::util::Status OnlpThermalDataSource::UpdateValues() {
  ASSIGN_OR_RETURN(ThermalInfo thermal_info, onlp_stub_->GetThermalInfo(thermal_oid_));

  // Onlp hw_state always populated.
  thermal_hw_state_ = thermal_info.GetHardwareState();

  // Other attributes are only valid if THERMAL is present. Return if thermal not
  // present.
  CHECK_RETURN_IF_FALSE(thermal_info.Present()) << "THERMAL is not present.";

  thermal_cur_temp_.AssignValue(static_cast<double>(thermal_info.GetThermalCurTemp())/1000.0);
  thermal_warn_temp_.AssignValue (static_cast<double>(thermal_info.GetThermalWarnTemp())/1000.0);
  thermal_error_temp_.AssignValue(static_cast<double>(thermal_info.GetThermalErrorTemp())/1000.0);
  thermal_shut_down_temp_.AssignValue(static_cast<double>(thermal_info.GetThermalShutDownTemp())/1000.0);

  return ::util::OkStatus();
}

::util::Status OnlpThermalDataSource::IsCapable(ThermalCaps thermal_caps) {
  bool is_capable;
  
  ASSIGN_OR_RETURN(ThermalInfo thermal_info, onlp_stub_->GetThermalInfo(thermal_oid_));
  is_capable = thermal_info.Capable(thermal_caps);
  CHECK_RETURN_IF_FALSE(is_capable) << "Expected Thermal capability is not present.";
  
  return ::util::OkStatus();
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
