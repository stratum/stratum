/*
 * Copyright 2019-present Open Networking Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef STRATUM_HAL_LIB_BCM_SDK_SDK_BUILD_FLAGS_H_
#define STRATUM_HAL_LIB_BCM_SDK_SDK_BUILD_FLAGS_H_

#define BCM_ESW_SUPPORT
#define BCM_PLATFORM_STRING "X86"
#define BCM_WARM_BOOT_SUPPORT
#define INCLUDE_KNET
#define INCLUDE_L3
#define LE_HOST 1
#define LINUX
#define LONGS_ARE_64BITS
#define PHYS_ADDRS_ARE_64BITS
#define PTRS_ARE_64BITS
#define SAL_BDE_DMA_MEM_DEFAULT 32
#define SAL_SPL_LOCK_ON_IRQ
#define SYS_BE_OTHER 0
#define SYS_BE_PACKET 0
#define SYS_BE_PIO 0
#define UNIX

#endif  // STRATUM_HAL_LIB_BCM_SDK_SDK_BUILD_FLAGS_H_
