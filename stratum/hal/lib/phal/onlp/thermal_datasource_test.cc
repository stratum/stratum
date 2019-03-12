// Copyright 2019 Edgecore Networks Corporation
// Phani Karanam <phani_karanam@edge-core.com>
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stratum/hal/lib/phal/onlp/thermal_datasource.h"

#include <memory>
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/test_util.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/test_utils/matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/glue/status/status_test_util.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;
using ::stratum::test_utils::StatusIs;

TEST(ThermalDatasourceTest, InitializeFailedNoThermal) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_UNPLUGGED;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillOnce(Return(OidInfo(mock_oid_info)));

  std::string error_message =
      "The THERMAL with OID 12345 is not currently present.";
  EXPECT_THAT(OnlpThermalDataSource::Make(12345, &mock_onlp_interface, nullptr),
              StatusIs(_, _, HasSubstr(error_message)));
}

TEST(ThermalDatasourceTest, InitializeThermalWithEmptyInfo) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillOnce(Return(OidInfo(mock_oid_info)));

  onlp_thermal_info_t mock_thermal_info = {};
  mock_thermal_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface, GetThermalInfo(12345))
      .Times(2)
      .WillRepeatedly(Return(ThermalInfo(mock_thermal_info)));

  ::util::StatusOr<std::shared_ptr<OnlpThermalDataSource>> result =
      OnlpThermalDataSource::Make(12345, &mock_onlp_interface, nullptr);
  ASSERT_OK(result);
  std::shared_ptr<OnlpThermalDataSource> thermal_datasource =
      result.ConsumeValueOrDie();
  EXPECT_NE(thermal_datasource.get(), nullptr);
}

TEST(ThermalDatasourceTest, GetThermalData) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillRepeatedly(Return(OidInfo(mock_oid_info)));

  onlp_thermal_info_t mock_thermal_info = {};
  mock_thermal_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;

  mock_thermal_info.mcelsius = 1111;
  mock_thermal_info.thresholds.warning = 2222;
  mock_thermal_info.thresholds.error = 3333;
  mock_thermal_info.thresholds.shutdown = 4444;
  mock_thermal_info.caps = (ONLP_THERMAL_CAPS_GET_TEMPERATURE
                            |ONLP_THERMAL_CAPS_GET_WARNING_THRESHOLD);

  EXPECT_CALL(mock_onlp_interface, GetThermalInfo(12345))
      .WillRepeatedly(Return(ThermalInfo(mock_thermal_info)));

  ::util::StatusOr<std::shared_ptr<OnlpThermalDataSource>> result =
      OnlpThermalDataSource::Make(12345, &mock_onlp_interface, nullptr);

  ASSERT_OK(result);

  std::shared_ptr<OnlpThermalDataSource> thermal_datasource =
      result.ConsumeValueOrDie();

  EXPECT_NE(thermal_datasource.get(), nullptr);

  // Update value and check attribute fields.
  EXPECT_OK(thermal_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());
  
  EXPECT_OK(thermal_datasource->IsCapable((ThermalCaps)(ONLP_THERMAL_CAPS_GET_TEMPERATURE
            |ONLP_THERMAL_CAPS_GET_WARNING_THRESHOLD)));

  EXPECT_THAT(thermal_datasource->GetThermalId(),
              ContainsValue<OnlpOid>(12345));

  EXPECT_THAT(thermal_datasource->GetThermalCurTemp(),
              ContainsValue<double>(1111 / 1000.0));
  EXPECT_THAT(thermal_datasource->GetThermalWarnTemp(),
              ContainsValue<double>(2222 / 1000.0));
  EXPECT_THAT(thermal_datasource->GetThermalErrorTemp(),
              ContainsValue<double>(3333 / 1000.0));
  EXPECT_THAT(thermal_datasource->GetThermalShutDownTemp(),
              ContainsValue<double>(4444 / 1000.0));

  EXPECT_THAT(
      thermal_datasource->GetThermalHardwareState(),
      ContainsValue(HwState_descriptor()->FindValueByName("HW_STATE_PRESENT")));
}

// TODO(phani-karanam): Add implementation.
// TEST(ThermalDatasourceTest, SetThermalData) {}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
