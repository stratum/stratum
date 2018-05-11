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


#ifndef STRATUM_HAL_LIB_PHAL_READER_WRITER_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_READER_WRITER_DATASOURCE_H_

#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "third_party/stratum/hal/lib/phal/datasource.h"
#include "third_party/stratum/hal/lib/phal/stringsource_interface.h"
#include "third_party/stratum/lib/macros.h"
#include "third_party/stratum/glue/status/status.h"
#include "third_party/stratum/glue/status/status_macros.h"
#include "third_party/stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace phal {

// A datasource that reads and parses the full contents of a string source into
// the given type. If the string source is settable, ReaderWriterDatasource will
// also convert the given type back into a string and write to the source.
template <typename T>
class ReaderWriterDataSource : public DataSource {
 public:
  // Constructs a new ReaderWriterDataSource that will read from/write to the
  // given string source.
  static std::shared_ptr<ReaderWriterDataSource> Make(
      std::unique_ptr<StringSourceInterface> source, CachePolicy* cache_type) {
    return std::shared_ptr<ReaderWriterDataSource>(
        new ReaderWriterDataSource<T>(std::move(source), cache_type));
  }

  // Returns the single attribute managed by this datasource.
  ManagedAttribute* GetAttribute() { return &attribute_; }

  // Alters this datasource so that any value read from the string source will
  // be passed through read_function, and any value written will be passed
  // through write_function before actually writing to the string source. These
  // functions should not have side effects, and should typically be inverses of
  // each other. I/O validation may also be done by returning an error Status.
  void AddModifierFunctions(
      const std::function<::util::StatusOr<T>(T)>& read_function,
      const std::function<::util::StatusOr<T>(T)>& write_function) {
    read_function_ = read_function;
    write_function_ = write_function;
  }

  // Reads from the stringsource and updates the attribute. This should not
  // usually be called directly. Instead, call UpdateValuesAndLock (from
  // DataSource).
  ::util::Status UpdateValues() override {
    ASSIGN_OR_RETURN(std::string string_value, source_->GetString());
    ASSIGN_OR_RETURN(T value, ParseValue(string_value));
    if (read_function_ != nullptr) {
      ASSIGN_OR_RETURN(value, read_function_(value));
    }
    attribute_.AssignValue(value);
    return ::util::OkStatus();
  }

 private:
  ReaderWriterDataSource(std::unique_ptr<StringSourceInterface> source,
                         CachePolicy* cache_type)
      : DataSource(cache_type), source_(std::move(source)) {
    if (source_->CanSet()) {
      attribute_.AddSetter(
          [this](T value) -> ::util::Status { return this->SetValue(value); });
    }
  }

  // Parses the given string into type T. Returns an error if the string cannot
  // be parsed. A separate template specialization for std::string is defined
  // below.
  inline ::util::StatusOr<T> ParseValue(const std::string& string_value) {
    T value;
    std::istringstream stream(string_value);
    stream >> value;
    CHECK_RETURN_IF_FALSE(stream.eof())
        << "Failed to parse requested type from input string \"" << string_value
        << "\".";
    return value;
  }

  virtual ::util::Status SetValue(T value) {
    T modded_value = value;
    if (write_function_ != nullptr) {
      ASSIGN_OR_RETURN(modded_value, write_function_(value));
    }
    std::ostringstream stream;
    stream << modded_value;
    CHECK_RETURN_IF_FALSE(stream.good())
        << "Failed to write " << modded_value << " to output string.";
    RETURN_IF_ERROR(source_->SetString(stream.str()));
    attribute_.AssignValue(value);
    return ::util::OkStatus();
  }

  std::unique_ptr<StringSourceInterface> source_;
  TypedAttribute<T> attribute_{this};
  std::function<::util::StatusOr<T>(T)> read_function_{};
  std::function<::util::StatusOr<T>(T)> write_function_{};
};

// Returns the passed string. ParseValue for std::string shoulds be a no-op, and
// we don't want the tokenization we would get from std::istringstream.
template <>
inline ::util::StatusOr<std::string>
ReaderWriterDataSource<std::string>::ParseValue(
    const std::string& string_value) {
  return string_value;
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_READER_WRITER_DATASOURCE_H_
