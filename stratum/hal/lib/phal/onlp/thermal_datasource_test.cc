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

class ThermalDatasourceTest : public ::testing::Test {
 public:
   void SetUp() override {
     id_ = 12345;
     oid_ = ONLP_THERMAL_ID_CREATE(id_);
   }

   int id_;             // Id for this THERMAL
   OnlpOid oid_;        // OID for this THERMAL (i.e. Type + Id)
   onlp_oid_hdr_t mock_oid_info_;
   MockOnlpWrapper mock_onlp_interface_;
};

TEST_F(ThermalDatasourceTest, InitializeThermalWithEmptyInfo) {
  mock_oid_info_.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface_, GetOidInfo(oid_))
      .WillOnce(Return(OidInfo(mock_oid_info_)));

  onlp_thermal_info_t mock_thermal_info = {};
  mock_thermal_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface_, GetThermalInfo(oid_))
      .Times(2)
      .WillRepeatedly(Return(ThermalInfo(mock_thermal_info)));

  ::util::StatusOr<std::shared_ptr<OnlpThermalDataSource>> result =
      OnlpThermalDataSource::Make(id_, &mock_onlp_interface_, nullptr);
  ASSERT_OK(result);
  std::shared_ptr<OnlpThermalDataSource> thermal_datasource =
      result.ConsumeValueOrDie();
  EXPECT_NE(thermal_datasource.get(), nullptr);
}

TEST_F(ThermalDatasourceTest, GetThermalData) {
  mock_oid_info_.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface_, GetOidInfo(oid_))
      .WillRepeatedly(Return(OidInfo(mock_oid_info_)));

  onlp_thermal_info_t mock_thermal_info = {};
  mock_thermal_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;

  mock_thermal_info.mcelsius = 1111;
  mock_thermal_info.thresholds.warning = 2222;
  mock_thermal_info.thresholds.error = 3333;
  mock_thermal_info.thresholds.shutdown = 4444;
  mock_thermal_info.caps = (ONLP_THERMAL_CAPS_GET_TEMPERATURE
                            |ONLP_THERMAL_CAPS_GET_WARNING_THRESHOLD);

  EXPECT_CALL(mock_onlp_interface_, GetThermalInfo(oid_))
      .WillRepeatedly(Return(ThermalInfo(mock_thermal_info)));

  ::util::StatusOr<std::shared_ptr<OnlpThermalDataSource>> result =
      OnlpThermalDataSource::Make(id_, &mock_onlp_interface_, nullptr);

  ASSERT_OK(result);

  std::shared_ptr<OnlpThermalDataSource> thermal_datasource =
      result.ConsumeValueOrDie();

  EXPECT_NE(thermal_datasource.get(), nullptr);

  // Update value and check attribute fields.
  EXPECT_OK(thermal_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());

  // Check capabilities
  EXPECT_THAT(thermal_datasource->GetCapTemp(), ContainsValue(true));
  EXPECT_THAT(thermal_datasource->GetCapWarnThresh(), ContainsValue(true));
  EXPECT_THAT(thermal_datasource->GetCapErrThresh(), ContainsValue(false));
  EXPECT_THAT(thermal_datasource->GetCapShutdownThresh(), ContainsValue(false));

  EXPECT_THAT(thermal_datasource->GetThermalId(),
              ContainsValue<int>(id_));

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
// TEST_F(ThermalDatasourceTest, SetThermalData) {}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
