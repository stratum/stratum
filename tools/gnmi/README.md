gNMI tool
----

### Usage

```
usage: bazel run //tools/gnmi:gnmi-cli -- [--grpc-addr GRPC_ADDR]
                    [--bool-val BOOL_VAL] [--int-val INT_VAL] [--uint-val UINT_VAL]
                    [--string-val STRING_VAL] [--float-val FLOAT_VAL]
                    [--bytes-val BYTES_VAL] [--interval INTERVAL] [--replace]
                    {get,set,sub-onchange,sub-sample,cap,del} path

Basic gNMI CLI

positional arguments:
  {get,set,sub-onchange,sub-sample,cap,del}     gNMI command
  path                                          gNMI Path

optional arguments:
  -h, --help            show this help message and exit
  --grpc-addr GRPC_ADDR
                        gNMI server address
  --bool-val BOOL_VAL   [SetRequest only] Set boolean value
  --int-val INT_VAL     [SetRequest only] Set int value
  --uint-val UINT_VAL   [SetRequest only] Set uint value
  --string-val STRING_VAL
                        [SetRequest only] Set string value
  --float-val FLOAT_VAL
                        [SetRequest only] Set float value
  --interval INTERVAL   [Sample subscribe only] Sample subscribe poll interval in ms
  --replace             [SetRequest only] Use replace instead of update
```

### Examples

```
# To get port index
bazel run //tools/gnmi:gnmi-cli -- get /interfaces/interface[name=1/1/1]/state/ifindex

# To set port health indicator
bazel run //tools/gnmi:gnmi-cli -- set /interfaces/interface[name=1/1/1]/config/health-indicator --string-val GOOD

# To subscribe one sample of port operation status per second
bazel run //tools/gnmi:gnmi-cli -- sub-sample /interfaces/interface[name=1/1/1]/state/oper-status --inverval 1000
```
