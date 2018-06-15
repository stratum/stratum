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

#ifndef STRATUM_GLUE_GTL_FLAT_HASH_MAP_H_
#define STRATUM_GLUE_GTL_FLAT_HASH_MAP_H_

#include <unordered_map>

namespace stratum {
namespace gtl {

// flat_hash_map<K,V,...> provides a hash map from K to V.
//
// The implementation of flat_hash_map is internal to Google. However,
// flat_hash_map implements the same interface as std::unordered_map, so
// we define an alias template to make the two synonyms.
// If an open source version of gtl makes flat_hash_map available, this
// file should be replaced.
template<
    class K,
    class V,
    class Hash = std::hash<K>,
    class KeyEqual = std::equal_to<K>,
    class Allocator = std::allocator< std::pair<const K, V> >
> using flat_hash_map = std::unordered_map<K, V, Hash, KeyEqual, Allocator>;

}  // namespace gtl
}  // namespace stratum

#endif  // STRATUM_GLUE_GTL_FLAT_HASH_MAP_H_