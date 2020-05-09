// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


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
