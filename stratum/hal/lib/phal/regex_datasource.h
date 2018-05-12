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


#ifndef STRATUM_HAL_LIB_PHAL_REGEX_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_REGEX_DATASOURCE_H_

#include <map>
#include <memory>

#include "base/basictypes.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/managed_attribute.h"
#include "stratum/hal/lib/phal/stringsource_interface.h"
#include "stratum/lib/macros.h"
#include "absl/memory/memory.h"
#include "util/regexp/re2/re2.h"

namespace stratum {
namespace hal {
namespace phal {

// A datasource implementation that parses attributes from a string based on a
// regex. All regex parsing is handled by RE2.
//
// Example usage:
//     std::unique_ptr<StringSourceInterface> contents = ...
//     // Regex matches: <int> blah blah blah <double>
//     std::string regex = "(\\d+) blah .* blah (\\d+.\\d+)"
//     auto datasource = RegexDataSource::Make(regex, std::move(contents),
//                                             new NoCache());
//     ASSIGN_OR_RETURN(ManagedAttribute* first_matching_group,
//                      datasource->GetAttribute<int32>(1);
//     ASSIGN_OR_RETURN(ManagedAttribute* second_matching_group,
//                      datasource->GetAttribute<double>(2);
// This regex would now parse a string like "1000 blah blah blah 99.99", writing
// the integer value 100 to first_matching_group and the double value 99.99 to
// second_matching_group.
//
class RegexDataSource : public DataSource {
 public:
  // Constructs a new RegexDataSource that will parse the given stringsource
  // using the given regex.
  static std::shared_ptr<RegexDataSource> Make(
      const std::string& regex,
      std::unique_ptr<StringSourceInterface> stringsource,
      CachePolicy* cache_type) {
    return std::shared_ptr<RegexDataSource>(
        new RegexDataSource(regex, std::move(stringsource), cache_type));
  }

  // Returns an attribute that contains the contents of the specified 1-indexed
  // capturing group from the regex. The attribute will be parsed as type T.
  // Only one attribute may be requested for each capturing group.
  template <typename T>
  ::util::StatusOr<ManagedAttribute*> GetAttribute(int capturing_group) {
    CHECK_RETURN_IF_FALSE(capturing_group > 0 &&
                          capturing_group <= regex_.NumberOfCapturingGroups())
        << "Capturing group " << capturing_group << " is not valid for regex \""
        << regex_.pattern() << "\".";
    CHECK_RETURN_IF_FALSE(args_[capturing_group - 1] == &dummy_arg_)
        << "Cannot create multiple attributes for a single regex capturing "
           "group.";
    fields_[capturing_group - 1] = absl::make_unique<TypedRegexField<T>>(this);
    args_[capturing_group - 1] = fields_[capturing_group - 1]->GetArg();
    return fields_[capturing_group - 1]->GetAttribute();
  }

  // Reads from the stringsource and updates all attributes. This should not
  // usually be called directly. Instead, call UpdateValuesAndLock (from
  // DataSource).
  ::util::Status UpdateValues() override;

 private:
  RegexDataSource(const std::string& regex,
                  std::unique_ptr<StringSourceInterface> stringsource,
                  CachePolicy* cache_type);

  // A helper class representing a single capture group that will be parsed from
  // the regex.
  class RegexField {
   public:
    virtual ~RegexField() {}
    virtual ManagedAttribute* GetAttribute() = 0;
    virtual RE2::Arg* GetArg() = 0;
    virtual void Update() = 0;
  };

  // An implementation of RegexField that takes advantage of RE2's builtin field
  // parsing to generate attributes of several different types.
  template <typename T>
  class TypedRegexField : public RegexField {
   public:
    explicit TypedRegexField(DataSource* parent)
        : attribute_(parent), arg_(&arg_val_) {}
    ManagedAttribute* GetAttribute() override { return &attribute_; }
    RE2::Arg* GetArg() override { return &arg_; }
    void Update() override {
      attribute_.AssignValue(arg_val_);
    }

   protected:
    TypedAttribute<T> attribute_;
    RE2::Arg arg_;
    T arg_val_;
  };

  const RE2 regex_;
  std::unique_ptr<StringSourceInterface> stringsource_;
  std::vector<RE2::Arg*> args_;
  std::vector<std::unique_ptr<RegexField>> fields_;
  // A dummy argument that we pass to RE2 for capture groups the user hasn't
  // requested. This can normally be done by passing nullptr directly, but this
  // doesn't work for RE2::FullMatchN.
  RE2::Arg dummy_arg_ {static_cast<void*>(nullptr)};
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_REGEX_DATASOURCE_H_
