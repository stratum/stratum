/*
 * Copyright 2020-present Open Networking Foundation
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


#ifndef STRATUM_HAL_LIB_BCM_SDKLT_MACROS_INCLUDE_H_
#define STRATUM_HAL_LIB_BCM_SDKLT_MACROS_INCLUDE_H_

extern "C" {
#include "shr/shr_error.h"

/*
 * Remapping for compatability between Broadcom SDKs and
 * improved reability in other files.
 */
typedef enum {
    BCM_E_NONE       = SHR_E_NONE,
    BCM_E_INTERNAL   = SHR_E_INTERNAL,
    BCM_E_MEMORY     = SHR_E_MEMORY,
    BCM_E_UNIT       = SHR_E_UNIT,
    BCM_E_PARAM      = SHR_E_PARAM,
    BCM_E_EMPTY      = SHR_E_EMPTY,
    BCM_E_FULL       = SHR_E_FULL,
    BCM_E_NOT_FOUND  = SHR_E_NOT_FOUND,
    BCM_E_EXISTS     = SHR_E_EXISTS,
    BCM_E_TIMEOUT    = SHR_E_TIMEOUT,
    BCM_E_BUSY       = SHR_E_BUSY,
    BCM_E_FAIL       = SHR_E_FAIL,
    BCM_E_DISABLED   = SHR_E_DISABLED,
    BCM_E_BADID      = SHR_E_BADID,
    BCM_E_RESOURCE   = SHR_E_RESOURCE,
    BCM_E_CONFIG     = SHR_E_CONFIG,
    BCM_E_UNAVAIL    = SHR_E_UNAVAIL,
    BCM_E_INIT       = SHR_E_INIT,
    BCM_E_PORT       = SHR_E_PORT,
    BCM_E_IO         = SHR_E_IO,
    BCM_E_ACCESS     = SHR_E_ACCESS,
    BCM_E_NO_HANDLER = SHR_E_NO_HANDLER,
    BCM_E_PARTIAL    = SHR_E_PARTIAL
} bcm_error_t;

inline const char* bcm_errmsg(int rv) {
    return shr_errmsg(rv);
}

#define BCM_SUCCESS(_expr) SHR_SUCCESS(_expr)
#define BCM_FAILURE(_expr) SHR_FAILURE(_expr)

}  // extern "C"

#endif // STRATUM_HAL_LIB_BCM_SDKLT_MACROS_INCLUDE_H_
