#include "stratum/hal/lib/phal/datasource_mock.h"

namespace google {
namespace hercules {
namespace hal {
namespace phal {
namespace {
class DataSourceMockPtr : public DataSource {
 public:
  static std::shared_ptr<DataSource> Make(DataSourceMock* datasource) {
    return std::shared_ptr<DataSource>(new DataSourceMockPtr(datasource));
  }

  ::util::Status UpdateValuesAndLock() override NO_THREAD_SAFETY_ANALYSIS {
    return datasource_->UpdateValuesAndLock();
  }

  ::util::Status LockAndFlushWrites() override NO_THREAD_SAFETY_ANALYSIS {
    return datasource_->LockAndFlushWrites();
  }

  void Unlock() override NO_THREAD_SAFETY_ANALYSIS {
    return datasource_->Unlock();
  }

 protected:
  ::util::Status UpdateValues() override { return ::util::OkStatus(); }

 private:
  explicit DataSourceMockPtr(DataSourceMock* datasource)
      : DataSource(new NoCache()), datasource_(datasource) {}
  DataSourceMock* datasource_;
};
}  // namespace

std::shared_ptr<DataSource> DataSourceMock::GetSharedPointer() {
  return DataSourceMockPtr::Make(this);
}
}  // namespace phal
}  // namespace hal
}  // namespace hercules
}  // namespace google
