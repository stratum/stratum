#!/bin/bash
#
# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
#
set -ex

# Files in this branch that are different from master.
CHANGED_FILES=$(git diff-tree --no-commit-id --name-status -r HEAD origin/master \
                  | grep -v "^D" \
                  | cut -f 2 \
                  | grep -F -e ".cc" -e ".h" || :
              )

# List of files that are already formatted.
read -r -d '\0' KNOWN_FILES << EOF
stratum/glue/gtl/cleanup_test.cc
stratum/glue/gtl/cleanup.h
stratum/glue/gtl/map_util_test.cc
stratum/glue/gtl/map_util.h
stratum/glue/gtl/source_location.h
stratum/glue/gtl/stl_util.h
stratum/glue/init_google.h
stratum/glue/status/posix_error_space.cc
stratum/glue/status/status_test_util.cc
stratum/hal/bin/barefoot/bf_pipeline_builder.cc
stratum/hal/bin/bcm/standalone/main.cc
stratum/hal/bin/np4intel/main.cc
stratum/hal/lib/barefoot/bf_chassis_manager_test.cc
stratum/hal/lib/barefoot/bf_chassis_manager.cc
stratum/hal/lib/barefoot/bf_chassis_manager.h
stratum/hal/lib/barefoot/bf_pal_interface.h
stratum/hal/lib/barefoot/bf_pal_mock.h
stratum/hal/lib/barefoot/bf_pal_wrapper.cc
stratum/hal/lib/barefoot/bf_pal_wrapper.h
stratum/hal/lib/barefoot/bf_pd_interface.h
stratum/hal/lib/barefoot/bf_pd_mock.h
stratum/hal/lib/barefoot/bf_pd_wrapper.cc
stratum/hal/lib/barefoot/bf_pd_wrapper.h
stratum/hal/lib/barefoot/bf_pipeline_utils.cc
stratum/hal/lib/barefoot/bf_pipeline_utils.h
stratum/hal/lib/barefoot/bf_switch.cc
stratum/hal/lib/barefoot/bf_switch.h
stratum/hal/lib/barefoot/test_main.cc
stratum/hal/lib/bcm/bcm_chassis_manager_mock.h
stratum/hal/lib/bcm/bcm_chassis_manager_test.cc
stratum/hal/lib/bcm/bcm_chassis_manager.h
stratum/hal/lib/bcm/bcm_chassis_ro_mock.h
stratum/hal/lib/bcm/bcm_global_vars.h
stratum/hal/lib/bcm/bcm_l2_manager.cc
stratum/hal/lib/bcm/bcm_switch_test.cc
stratum/hal/lib/bcm/sdk/macros.h
stratum/hal/lib/bcm/sdk/sdk_build_flags.h
stratum/hal/lib/bcm/sdklt/bcm_diag_shell.cc
stratum/hal/lib/bcm/sdklt/bcm_sdk_wrapper.h
stratum/hal/lib/bcm/sdklt/macros.h
stratum/hal/lib/bcm/utils.cc
stratum/hal/lib/common/certificate_management_service.h
stratum/hal/lib/common/client_sync_reader_writer.h
stratum/hal/lib/common/config_monitoring_service.cc
stratum/hal/lib/common/config_monitoring_service.h
stratum/hal/lib/common/diag_service.h
stratum/hal/lib/common/file_service.h
stratum/hal/lib/common/p4_service_test.cc
stratum/hal/lib/common/p4_service.h
stratum/hal/lib/common/yang_parse_tree_paths.h
stratum/hal/lib/dummy/dummy_global_vars.h
stratum/hal/lib/np4intel/np4_chassis_manager_test.cc
stratum/hal/lib/np4intel/np4_chassis_manager.cc
stratum/hal/lib/np4intel/np4_chassis_manager.h
stratum/hal/lib/np4intel/np4_switch.cc
stratum/hal/lib/np4intel/np4_switch.h
stratum/hal/lib/np4intel/test_main.cc
stratum/hal/lib/p4/p4_write_request_differ.h
stratum/hal/lib/phal/adapter.cc
stratum/hal/lib/phal/adapter.h
stratum/hal/lib/phal/attribute_database_mock.h
stratum/hal/lib/phal/attribute_group.cc
stratum/hal/lib/phal/datasource_mock.cc
stratum/hal/lib/phal/onlp/onlp_event_handler_test.cc
stratum/hal/lib/phal/onlp/onlp_fan_datasource_test.cc
stratum/hal/lib/phal/onlp/onlp_led_datasource_test.cc
stratum/hal/lib/phal/onlp/onlp_phal_mock.h
stratum/hal/lib/phal/onlp/onlp_phal_test.cc
stratum/hal/lib/phal/onlp/onlp_phal.cc
stratum/hal/lib/phal/onlp/onlp_phal.h
stratum/hal/lib/phal/onlp/onlp_psu_datasource_test.cc
stratum/hal/lib/phal/onlp/onlp_sfp_configurator_test.cc
stratum/hal/lib/phal/onlp/onlp_sfp_configurator.cc
stratum/hal/lib/phal/onlp/onlp_sfp_configurator.h
stratum/hal/lib/phal/onlp/onlp_sfp_datasource_test.cc
stratum/hal/lib/phal/onlp/onlp_switch_configurator_test.cc
stratum/hal/lib/phal/onlp/onlp_thermal_datasource_test.cc
stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h
stratum/hal/lib/phal/onlp/onlp_wrapper.h
stratum/hal/lib/phal/optics_adapter_test.cc
stratum/hal/lib/phal/phal_backend_interface.h
stratum/hal/lib/phal/phal_cli.cc
stratum/hal/lib/phal/phal_test.cc
stratum/hal/lib/phal/phal.cc
stratum/hal/lib/phal/phal.h
stratum/hal/lib/phal/phaldb_service.h
stratum/hal/lib/phal/sfp_adapter_test.cc
stratum/hal/lib/phal/sfp_adapter.cc
stratum/hal/lib/phal/sfp_adapter.h
stratum/hal/lib/phal/tai/tai_interface_mock.h
stratum/hal/lib/phal/tai/tai_interface.h
stratum/hal/lib/phal/tai/tai_optics_datasource_test.cc
stratum/hal/lib/phal/tai/tai_optics_datasource.h
stratum/hal/lib/phal/tai/tai_phal_test.cc
stratum/hal/lib/phal/tai/tai_phal.cc
stratum/hal/lib/phal/tai/tai_phal.h
stratum/hal/lib/phal/tai/tai_switch_configurator_test.cc
stratum/hal/lib/phal/tai/tai_switch_configurator.cc
stratum/hal/lib/phal/tai/tai_switch_configurator.h
stratum/hal/lib/phal/tai/taish_client.cc
stratum/hal/lib/phal/tai/taish_client.h
stratum/hal/lib/phal/test_util.h
stratum/hal/lib/pi/pi_node.cc
stratum/hal/lib/pi/pi_node.h
stratum/hal/stub/embedded/main.cc
stratum/lib/constants.h
stratum/lib/libcproxy/libcwrapper.cc
stratum/lib/libcproxy/libcwrapper.h
stratum/lib/libcproxy/passthrough_proxy.cc
stratum/lib/libcproxy/passthrough_proxy.h
stratum/lib/test_utils/p4_proto_builders.cc
stratum/p4c_backends/common/backend_extension_interface.h
stratum/p4c_backends/common/backend_pass_manager.h
stratum/p4c_backends/common/midend_interface.h
stratum/p4c_backends/common/p4c_front_mid_interface.h
stratum/p4c_backends/fpm/bcm/bcm_target_info.cc
stratum/p4c_backends/fpm/bcm/bcm_target_info.h
stratum/p4c_backends/fpm/bcm/bcm_tunnel_optimizer.h
stratum/p4c_backends/fpm/condition_inspector.h
stratum/p4c_backends/fpm/hidden_static_mapper.h
stratum/p4c_backends/fpm/internal_action.h
stratum/p4c_backends/fpm/meta_key_mapper.cc
stratum/p4c_backends/fpm/pipeline_optimizer.h
stratum/p4c_backends/fpm/simple_hit_inspector.h
stratum/p4c_backends/fpm/table_type_mapper.h
stratum/p4c_backends/fpm/target_info.cc
stratum/p4c_backends/fpm/target_info.h
stratum/p4c_backends/fpm/tunnel_optimizer_interface.h
stratum/p4c_backends/fpm/tunnel_type_mapper.h
stratum/p4c_backends/test/test_p4c_main.cc
stratum/p4c_backends/test/test_target_info.cc
stratum/p4c_backends/test/test_target_info.h
stratum/testing/tests/bcm_sim_test_fixture.cc
stratum/testing/tests/bcm_sim_test_fixture.h
stratum/testing/tests/bcm_sim_test.cc
stratum/testing/tests/test_main.cc
stratum/tools/gnmi/gnmi_cli.cc
\0
EOF

echo "$CHANGED_FILES" | xargs -t -n1 clang-format --style=file -i
echo "$KNOWN_FILES" | xargs -t -n1 clang-format --style=file -i

# Report which files need to be fixed.
git update-index --refresh
git diff
