# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

"""
    Currently, Stratum only supports the x86 architecture for testing
    and embedded system. We may support more architectures in the future.

    EMBEDDED_ARCHES is the set of supported embedded systems
      (e.g. switch platforms)
    HOST_ARCHES is the set of support host architectures
      (e.g. for running against a simulator or testing)
"""
EMBEDDED_ARCHES = [ "x86" ]
HOST_ARCHES = [ "x86" ]
STRATUM_INTERNAL = [ "//stratum:__subpackages__" ]