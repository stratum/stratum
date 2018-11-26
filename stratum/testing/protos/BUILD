package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache v2

load(
    "//platforms/networking/sandblaze/portage:build_defs.bzl",
    "sc_package",
)
load(
    "//platforms/networking/hercules/p4c_backend/common:build_defs.bzl",
    "p4_hercules_config",
)

sc_package(
    name = "hercules_test_protos",
    data = glob(["*.txt"]) + glob(["*.bin"]),
)

filegroup(
    name = "bcm_sim_test_protos",
    srcs = [
        "test_chassis_config_generic_trident2_40g_hercules.pb.txt",
        "test_p4_info_hercules_tor.pb.txt",
        "test_p4_pipeline_config_hercules_tor.pb.bin",
        "test_p4_pipeline_config_hercules_tor.pb.txt",
        "test_write_request_hercules_tor_generic_trident2_generic_tomahawk.pb.txt",
    ],
)

# the set of protos we use for standalone testing on HW testbeds.
filegroup(
    name = "standalone_testing_protos",
    srcs = [
        "test_p4_info_hercules_b4.pb.txt",
        "test_p4_info_hercules_fbr_s2.pb.txt",
        "test_p4_info_hercules_fbr_s3.pb.txt",
        "test_p4_info_hercules_middleblock.pb.txt",
        "test_p4_info_hercules_spine.pb.txt",
        "test_p4_info_hercules_tor.pb.txt",
        "test_p4_pipeline_config_hercules_b4.pb.bin",
        "test_p4_pipeline_config_hercules_b4.pb.txt",
        "test_p4_pipeline_config_hercules_fbr_s2.pb.bin",
        "test_p4_pipeline_config_hercules_fbr_s2.pb.txt",
        "test_p4_pipeline_config_hercules_fbr_s3.pb.bin",
        "test_p4_pipeline_config_hercules_fbr_s3.pb.txt",
        "test_p4_pipeline_config_hercules_middleblock.pb.bin",
        "test_p4_pipeline_config_hercules_middleblock.pb.txt",
        "test_p4_pipeline_config_hercules_spine.pb.bin",
        "test_p4_pipeline_config_hercules_spine.pb.txt",
        "test_p4_pipeline_config_hercules_tor.pb.bin",
        "test_p4_pipeline_config_hercules_tor.pb.txt",
        "test_write_request_hercules_spine_generic_trident2_1.pb.txt",
        "test_write_request_hercules_spine_generic_trident2_2.pb.txt",
        "test_write_request_hercules_spine_generic_trident2_3.pb.txt",
        "test_write_request_hercules_tor_generic_trident2_generic_tomahawk.pb.txt",
    ],
)

filegroup(
    name = "protos_to_validate",
    srcs = [
        "test_p4_info_hercules_b4.pb.txt",
        "test_p4_info_hercules_fbr_s2.pb.txt",
        "test_p4_info_hercules_fbr_s3.pb.txt",
        "test_p4_info_hercules_middleblock.pb.txt",
        "test_p4_info_hercules_spine.pb.txt",
        "test_p4_info_hercules_tor.pb.txt",
        "test_p4_pipeline_config_hercules_b4.pb.bin",
        "test_p4_pipeline_config_hercules_b4.pb.txt",
        "test_p4_pipeline_config_hercules_fbr_s2.pb.bin",
        "test_p4_pipeline_config_hercules_fbr_s2.pb.txt",
        "test_p4_pipeline_config_hercules_fbr_s3.pb.bin",
        "test_p4_pipeline_config_hercules_fbr_s3.pb.txt",
        "test_p4_pipeline_config_hercules_middleblock.pb.bin",
        "test_p4_pipeline_config_hercules_middleblock.pb.txt",
        "test_p4_pipeline_config_hercules_spine.pb.bin",
        "test_p4_pipeline_config_hercules_spine.pb.txt",
        "test_p4_pipeline_config_hercules_tor.pb.bin",
        "test_p4_pipeline_config_hercules_tor.pb.txt",
    ],
)

cc_test(
    name = "p4info_files_test",
    srcs = [
        "p4info_parse_test.cc",
    ],
    data = [
        "hercules_b4_p4_info.pb.txt",
        "hercules_b4_p4_pipeline.pb.bin",
        "hercules_b4_p4_pipeline.pb.txt",
        "hercules_fbr_s2_p4_info.pb.txt",
        "hercules_fbr_s2_p4_pipeline.pb.bin",
        "hercules_fbr_s2_p4_pipeline.pb.txt",
        "hercules_fbr_s3_p4_info.pb.txt",
        "hercules_fbr_s3_p4_pipeline.pb.bin",
        "hercules_fbr_s3_p4_pipeline.pb.txt",
        "hercules_middleblock_p4_info.pb.txt",
        "hercules_middleblock_p4_pipeline.pb.bin",
        "hercules_middleblock_p4_pipeline.pb.txt",
        "hercules_minicluster_s2_p4_info.pb.txt",
        "hercules_minicluster_s2_p4_pipeline.pb.bin",
        "hercules_minicluster_s2_p4_pipeline.pb.txt",
        "hercules_minicluster_s3_p4_info.pb.txt",
        "hercules_minicluster_s3_p4_pipeline.pb.bin",
        "hercules_minicluster_s3_p4_pipeline.pb.txt",
        "hercules_spine_p4_info.pb.txt",
        "hercules_spine_p4_pipeline.pb.bin",
        "hercules_spine_p4_pipeline.pb.txt",
        "hercules_tor_p4_info.pb.txt",
        "hercules_tor_p4_pipeline.pb.bin",
        "hercules_tor_p4_pipeline.pb.txt",
        ":protos_to_validate",
    ],
    deps = [
        "//base",
        "//platforms/networking/hercules/hal/lib/common:utils",
        "//platforms/networking/hercules/hal/lib/p4:p4_info_manager",
        "//platforms/networking/hercules/hal/lib/p4:p4_table_mapper",
        "//testing/base/public:gunit_main_no_google3",
        "//absl/memory",
        "//absl/strings",
        "//protobuf",
        "//sandblaze/p4lang:p4runtime_v1_proto",
    ],
)

p4_hercules_config(
    name = "hercules_tor",
    src = "//platforms/networking/orion/p4/spec:tor.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    annotation_maps = [
        "//platforms/networking/hercules/p4c_backend/switch:annotation_map_files",
    ],
    out_p4_info = "hercules_tor_p4_info.pb.txt",
    out_p4_pipeline_binary = "hercules_tor_p4_pipeline.pb.bin",
    out_p4_pipeline_text = "hercules_tor_p4_pipeline.pb.txt",
    parser_map = "//platforms/networking/hercules/p4c_backend/switch:parser_map_files",
)

p4_hercules_config(
    name = "hercules_spine",
    src = "//platforms/networking/orion/p4/spec:spine.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    annotation_maps = [
        "//platforms/networking/hercules/p4c_backend/switch:annotation_map_files",
    ],
    out_p4_info = "hercules_spine_p4_info.pb.txt",
    out_p4_pipeline_binary = "hercules_spine_p4_pipeline.pb.bin",
    out_p4_pipeline_text = "hercules_spine_p4_pipeline.pb.txt",
    parser_map = "//platforms/networking/hercules/p4c_backend/switch:parser_map_files",
)

p4_hercules_config(
    name = "hercules_middleblock",
    src = "//platforms/networking/orion/p4/spec:middleblock.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    annotation_maps = [
        "//platforms/networking/hercules/p4c_backend/switch:annotation_map_files",
    ],
    out_p4_info = "hercules_middleblock_p4_info.pb.txt",
    out_p4_pipeline_binary = "hercules_middleblock_p4_pipeline.pb.bin",
    out_p4_pipeline_text = "hercules_middleblock_p4_pipeline.pb.txt",
    parser_map = "//platforms/networking/hercules/p4c_backend/switch:parser_map_files",
)

p4_hercules_config(
    name = "hercules_fbr_s2",
    src = "//platforms/networking/orion/p4/spec:fbr_s2.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    annotation_maps = [
        "//platforms/networking/hercules/p4c_backend/switch:annotation_map_files",
    ],
    out_p4_info = "hercules_fbr_s2_p4_info.pb.txt",
    out_p4_pipeline_binary = "hercules_fbr_s2_p4_pipeline.pb.bin",
    out_p4_pipeline_text = "hercules_fbr_s2_p4_pipeline.pb.txt",
    parser_map = "//platforms/networking/hercules/p4c_backend/switch:parser_map_files",
)

p4_hercules_config(
    name = "hercules_fbr_s3",
    src = "//platforms/networking/orion/p4/spec:fbr_s3.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    annotation_maps = [
        "//platforms/networking/hercules/p4c_backend/switch:annotation_map_files",
    ],
    out_p4_info = "hercules_fbr_s3_p4_info.pb.txt",
    out_p4_pipeline_binary = "hercules_fbr_s3_p4_pipeline.pb.bin",
    out_p4_pipeline_text = "hercules_fbr_s3_p4_pipeline.pb.txt",
    parser_map = "//platforms/networking/hercules/p4c_backend/switch:parser_map_files",
)

p4_hercules_config(
    name = "hercules_b4",
    src = "//platforms/networking/orion/p4/spec:b4.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    annotation_maps = [
        "//platforms/networking/hercules/p4c_backend/switch:annotation_map_files",
    ],
    out_p4_info = "hercules_b4_p4_info.pb.txt",
    out_p4_pipeline_binary = "hercules_b4_p4_pipeline.pb.bin",
    out_p4_pipeline_text = "hercules_b4_p4_pipeline.pb.txt",
    parser_map = "//platforms/networking/hercules/p4c_backend/switch:parser_map_files",
)

p4_hercules_config(
    name = "hercules_minicluster_s2",
    src = "//platforms/networking/orion/p4/spec:minicluster_s2.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    annotation_maps = [
        "//platforms/networking/hercules/p4c_backend/switch:annotation_map_files",
    ],
    out_p4_info = "hercules_minicluster_s2_p4_info.pb.txt",
    out_p4_pipeline_binary = "hercules_minicluster_s2_p4_pipeline.pb.bin",
    out_p4_pipeline_text = "hercules_minicluster_s2_p4_pipeline.pb.txt",
    parser_map = "//platforms/networking/hercules/p4c_backend/switch:parser_map_files",
)

p4_hercules_config(
    name = "hercules_minicluster_s3",
    src = "//platforms/networking/orion/p4/spec:minicluster_s3.p4",
    hdrs = [
        "//platforms/networking/orion/p4/spec:p4behaviors",
    ],
    annotation_maps = [
        "//platforms/networking/hercules/p4c_backend/switch:annotation_map_files",
    ],
    out_p4_info = "hercules_minicluster_s3_p4_info.pb.txt",
    out_p4_pipeline_binary = "hercules_minicluster_s3_p4_pipeline.pb.bin",
    out_p4_pipeline_text = "hercules_minicluster_s3_p4_pipeline.pb.txt",
    parser_map = "//platforms/networking/hercules/p4c_backend/switch:parser_map_files",
)
