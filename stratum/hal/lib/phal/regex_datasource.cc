// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "stratum/hal/lib/phal/regex_datasource.h"

#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {

RegexDataSource::RegexDataSource(
    const std::string& regex,
    std::unique_ptr<StringSourceInterface> stringsource,
    CachePolicy* cache_type)
    : DataSource(cache_type),
      regex_(std::string(regex)),
      stringsource_(std::move(stringsource)),
      args_(regex_.NumberOfCapturingGroups(), &dummy_arg_),
      fields_(regex_.NumberOfCapturingGroups()) {}

::util::Status RegexDataSource::UpdateValues() {
  ASSIGN_OR_RETURN(std::string str, stringsource_->GetString());
  bool regex_matches;
  regex_matches = RE2::FullMatchN(str, regex_, args_.data(),
                                  regex_.NumberOfCapturingGroups());
  if (!regex_matches) {
    // This error could be due to either the regex failing to match, or the
    // capture groups failing to parse. Disambiguate.
    if (args_.empty() || !RE2::FullMatch(str, regex_)) {
      return MAKE_ERROR() << "Could not parse \"" << str << "\" with regex \""
                          << regex_.pattern() << "\".";
    } else {
      return MAKE_ERROR() << "Could not parse \"" << str << "\" with regex \""
                          << regex_.pattern()
                          << "\" into attributes of the requested types.";
    }
  }
  for (auto& field : fields_) {
    if (field) {
      field->Update();
    }
  }
  return ::util::OkStatus();
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
