/*
 * Copyright 2018 Open Networking Foundation
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

#ifndef STRATUM_GLUE_GTL_FLAT_HASH_SET_H_
#define STRATUM_GLUE_GTL_FLAT_HASH_SET_H_

#include <unordered_set>

namespace stratum {
namespace gtl {

// flat_hash_set<T,...> provides a hash set of T.
//
// The implementation of flat_hash_set is internal to Google. However,
// flat_hash_set implements the same interface as std::unordered_set, so
// we define an alias template to make the two synonyms.
// If an open source version of gtl makes flat_hash_set available, this
// file should be replaced.
template<
    class T,
    class Hash = std::hash<T>,
    class Pred = std::equal_to<T>,
    class Alloc = std::allocator<T>
> using flat_hash_set = std::unordered_set<T, Hash, Pred, Alloc>;

}  // namespace gtl
}  // namespace stratum

#endif  // STRATUM_GLUE_GTL_FLAT_HASH_SET_H_