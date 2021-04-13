# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

def stratum_platform_select(host = None, ppc = None, x86 = None, default = None):
    """Public macro to alter blaze rules based on the platform architecture.

    Generates a blaze select(...) statement that can be used in most contexts to
    alter a blaze rule based on the target platform architecture. If no selection
    is provided for a given platform, {default} is used instead. A specific value
    or default must be provided for every target platform.

    Args:
      host: The value to use for host builds.
      ppc: The value to use for ppc builds.
      x86: The value to use for x86 builds.
      default: The value to use for any of {host,ppc,x86} that isn't specified.

    Returns:
      The requested selector.
    """
    if default == None and (host == None or ppc == None or x86 == None):
        fail("Missing a select value for at least one platform in " +
             "stratum_platform_select. Please add.")
    config_label_prefix = "//stratum:stratum_"
    return select({
        "//conditions:default": (host or default),
        config_label_prefix + "ppc": (ppc or default),
        config_label_prefix + "x86": (x86 or default),
    })

# Generates an stratum_platform_select based on a textual list of arches.
def stratum_platform_filter(value, default, arches):
    return stratum_platform_select(
        host = value if "host" in arches else default,
        ppc = value if "ppc" in arches else default,
        x86 = value if "x86" in arches else default,
    )

def stratum_platform_alias(
        name,
        host = None,
        ppc = None,
        x86 = None,
        default = None,
        visibility = None):
    """Public macro to create an alias that changes based on target arch.

    Generates a blaze alias that will select the appropriate target. If no
    selection is provided for a given platform and no default is set, a
    dummy default target is used instead.

    Args:
      name: The name of the alias target.
      host: The result of the alias for host builds.
      ppc: The result of the alias for ppc builds.
      x86: The result of the alias for x86 builds.
      default: The result of the alias for any of {host,ppc,x86} that isn't
               specified.
      visibility: The visibility of the alias target.
    """
    native.alias(
        name = name,
        actual = stratum_platform_select(
            default = default or "//stratum/portage:dummy",
            host = host,
            ppc = ppc,
            x86 = x86,
        ),
        visibility = visibility,
    )
