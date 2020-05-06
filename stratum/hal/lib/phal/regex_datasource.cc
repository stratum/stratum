// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


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
