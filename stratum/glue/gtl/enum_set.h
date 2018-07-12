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

#ifndef STRATUM_GLUE_GTL_ENUM_SET_H_
#define STRATUM_GLUE_GTL_ENUM_SET_H_

#include <set>

namespace stratum {
namespace gtl {

// enum_set<T,...> provides a set of T.
//
// The implementation of flat_hash_set is internal to Google, and it
// accepts enums as the type T for the set. However, unordered_set does
// not support enums as the type T until C++14. This can be removed when
// either (a) Google open sources flat_hash_set or (b) the project moves
// to C++14.
template<
    class T,
    class Pred = std::equal_to<T>,
    class Alloc = std::allocator<T>
> using enum_set = std::set<T, Pred, Alloc>;

}  // namespace gtl
}  // namespace stratum

#endif  // STRATUM_GLUE_GTL_ENUM_SET_H_