/*
 * Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Nicira, Inc.
 * Copyright (c) 2022 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Adapted from ovs_strzcpy and ovs_parse_key_value.

#ifndef GNMI_CTL_UTILS_H
#define GNMI_CTL_UTILS_H 1

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool client_parse_key_value(char **stringp, char **keyp, char **valuep);

void client_strzcpy(char *dst, const char *src, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* GNMI_CTL_UTILS_H */
