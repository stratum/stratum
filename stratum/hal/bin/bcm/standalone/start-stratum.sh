#!/bin/bash
set -ex

rmmod linux_ngknet && rmmod linux_ngbde || true
insmod linux_ngbde.ko && insmod linux_ngknet.ko
sleep 1

./stratum_bcm \
    -external_stratum_urls=0.0.0.0:28000 \
    -persistent_config_dir=/tmp/stratum \
    -base_bcm_chassis_map_file=/stratum_configs/chassis_map.pb.txt \
    -chassis_config_file=/stratum_configs/accton7710_bcm_chassis_config_onos_demo.pb.txt \
    -bcm_sdk_config_file=/stratum_configs/AS7712-onos-demo.config.yml \
    -forwarding_pipeline_configs_file=/tmp/stratum/pipeline_cfg.pb.txt \
    -write_req_log_file=/tmp/stratum/p4_writes.pb.txt \
    -bcm_serdes_db_proto_file=/stratum_configs/dummy_serdes_db.pb.txt \
    -bcm_hardware_specs_file=/stratum_configs/bcm_hardware_specs.pb.txt \
    -bcm_sdk_checkpoint_dir=/tmp/bcm_chkpt \
    -colorlogtostderr \
    -logtosyslog=false \
    -stderrthreshold=1 \
    -grpc_max_recv_msg_size=256 \
    -knet_rx_poll_timeout_ms=2000 \
    -onlp_polling_interval_ms=2000 \
    -v=0 \
    -phal_config_path=/dev/null
