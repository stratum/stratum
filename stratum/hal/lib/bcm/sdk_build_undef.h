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


// This file is used to undefine preprocessor definitions specific to
// Broadcom SDK that conflict with definitions in depot3/google3.
// It should be included at the start and at the end of a list of
// SDK header files within a depot3/google3 source file.
// There is no #ifdef guard in order to allow for multiple inclusions.
// NOLINT(build/header_guard)

#ifdef LOG_FATAL
#undef LOG_FATAL
#endif

#ifdef LOG_ERROR
#undef LOG_ERROR
#endif

#ifdef LOG_WARN
#undef LOG_WARN
#endif

#ifdef LOG_INFO
#undef LOG_INFO
#endif

#ifdef LOG_VERBOSE
#undef LOG_VERBOSE
#endif

#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif

#ifdef PRIu32
#undef PRIu32
#endif

#ifdef PRIx32
#undef PRIx32
#endif

#ifdef PRIu64
#undef PRIu64
#endif
