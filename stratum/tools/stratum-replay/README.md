<!--
Copyright 2020-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

P4Runtime write request replay tool
====

This tool replay P4Runtime write requests to a Stratum device with a given
Stratum P4Runtime write request log.

# Getting started

## Step 1 - Prerequisites for Stratum

Before using this tool, make sure you enabled logging for P4Runtime write requests
and sets the pipeline config path flag.

By default, Stratum will use the following flags for P4Runtime write log and
the pipeline config.

```
-write_req_log_file=/var/log/stratum/p4_writes.pb.txt
-forwarding_pipeline_configs_file=/var/run/stratum/pipeline_cfg.pb.txt
```

If you override any flags above, make sure to use a non-empty and valid path.

Copy those files to your laptop/server so we can use it later.

Check step 1.1 below if you are running a containerized Stratum.

Go to step 2 if you already have those files.

### Step 1.1 - Obtaining files from the Stratum container

To copy files from a docker container,  we get the container ID so we know which
container we should access.

```
$ docker ps | grep stratum-bf
4c615277261d  stratumproject/stratum-bf:9.2.0-4.14.49-OpenNetworkLinux  "/usr/bin/stratum-st…"  5 days ago  Up 5 days
```

The `4c615277261d` is the container ID we need.

Next, we can use `docker cp` command to copy files we need

```
$ docker cp 4c615277261d:/var/log/stratum/p4_writes.pb.txt .
$ docker cp 4c615277261d:/var/run/stratum/pipeline_cfg.pb.txt .
```

You should be able to see those files in the current working directory.

```
$ ls
p4_writes.pb.txt  pipeline_cfg.pb.txt
```

Copy those files to your laptop or the place you are going to run stratum-replay tool.

## Step 2 - Replay the pipeline, and P4Runtime writes

We provide a container image that includes a prebuilt stratum-replay binary.

To use it, you can run the following commands:

```
docker run \
  -v $PWD:$PWD \
  -w $PWD \
  stratumproject/stratum-replay \
  -grpc-addr="ip-of-switch-to-replay-on:9339" \
  -pipeline-cfg pipeline_cfg.pb.txt \
  p4_writes.pb.txt
```

## Step 3 - Check the result

You will see the following message if every P4Runtime write succeeded

```
Done
```

However, you will get the following messages if something goes wrong:

```
Failed to send P4Runtime write request: [Error detail]
```

This message means there is a P4Runtime error when the tool is trying to send a write request,
but there is not error when writing the same request to the original switch.

You may also get some warning message such as:

```
Expect to get an error, but the request succeeded.
Expected error: [Error message]
Request: [Request body]
```

This means there is an error shown in the log, which means we should expect an
error when sending the write request, but we don't get any error.

The third warning message you can get is:

```
The expected error message is different from the actual error message:
Expected: [Error message]
Actual: [Error message]
```

This message means there is an error in the log, and the replay tool also get an error
after sending a write request, but the error message is different.

Errors and warnings above can be caused by the wrong software version
(e.g., using a different version of stratum) or using the wring write request
for a given pipeline config file.

# Usage and available options:

`stratum-replay [options] [p4runtime write log file]`

```
-device_id: The device ID (default: 1)
-election_id: Election ID for arbitration update (high,low). (default: "0,1")
-grpc_addr: Stratum gRPC address (default: "127.0.0.1:9339")
-pipeline_cfg: The pipeline config file (default: "pipeline.pb.bin")
-ca_cert: CA certificate(optional), will use insecure credentials if empty (default: "")
-client_cert: Client certificate (optional) (default: "")
-client_key: Client key (optional) (default: "")
```
