load(
    "//bazel:rules.bzl",
    "SANDBLAZE_INTERNAL",
    "stratum_package",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = SANDBLAZE_INTERNAL,
)

genrule(
    name = "phal_config_generic_trident2",
    outs = ["legacy_phal_init_config_generic_trident2_hercules.pb.txt"],
    cmd = (
        "$(location //stratum/hal/config/tools:google_config_generator) " +
        "--platform=generic_trident2 --config_type=phal --output=$@"
    ),
    tools = ["//stratum/hal/config/tools:google_config_generator"],
)

genrule(
    name = "phal_config_generic_trident2",
    outs = ["legacy_phal_init_config_generic_trident2_hercules.pb.txt"],
    cmd = (
        "$(location //stratum/hal/config/tools:google_config_generator) " +
        "--platform=generic_trident2 --config_type=phal --output=$@"
    ),
    tools = ["//stratum/hal/config/tools:google_config_generator"],
)

stratum_package(
    name = "hercules_config_files",
    data = glob(["*.txt"]) + glob(["*.bin"]) + [
        "legacy_phal_init_config_generic_trident2_hercules.pb.txt",
        "legacy_phal_init_config_generic_trident2_hercules.pb.txt",
    ],
)

filegroup(
    name = "bcm_sim_config_files",
    srcs = [
        "base_bcm_chassis_map_generic_tomahawk_hercules.pb.txt",
        "base_bcm_chassis_map_generic_trident2_hercules.pb.txt",
        "base_bcm_chassis_map_generic_trident2_hercules.pb.txt",
        "bcm_hardware_specs.pb.txt",
        "generic_trident2_bcm_serdes_db_hercules.pb.bin",
        "generic_trident2_bcm_serdes_db_hercules.pb.bin",
    ],
)
