# The gNMI command line client manual
To get/set value from/to Stratum YANG tree using gNMI command line client use
this instruction.

## General information
### CLI arguments
Supported commands format:
```
./gnmi_request.sh \
    [path-to-binary] \
    [request-type] \
    [yang-path] \
    [identifier] \
    OPTIONAL: [value-to-set]
```
The arguments here have the following meaning:
* `path-to-binary` - the path to gNMI CLI executable file;
* `request-type` - one of standard gNMI requests (see "Supported gNMI requests" section);
* `yang-path` - the YANG tree node path;
* `identifier` - the node, port and channel identifiers, splitted with slash ('/');
* `value-to-set` - optional argument when setting value, see examples.

### Supported gNMI requests
* `get` perform gNMI `Get` request on the requested `yang-path`.
* `set-int` perform gNMI `Set` request on the requested path with new integer value argument.
* `set-uint` perform gNMI `Set` request on the requested path with new unsigned integer value argument.
* `set-string` perform gNMI `Set` request on the requested path with new string value argument.
* `set-decimal` perform gNMI `Set` request on the requested path with new string value argument.
* `subscribe-change` perform gNMI `Subscribe` request on the requested path using `ON_CHANGE` subscription.
* `subscribe-sample` perform gNMI `Subscribe` request on the requested path using `SAMPLE` subscription.
* `capabilities` perform gNMI `Capabilities` request with empty proto (no extensions).

__Note__. Subscription requests have default infinite duration.

## Build and run
### If you have a gNMI CLI binary
Simply run `gnmi_request.sh`:
```
./gnmi_request.sh [path-to-gNMI-cli-client] \
    [command] [yang-path] [node/port/channel]
```
For example:
```
./gnmi_request.sh gnmi_cli \
    set-uint /components/component/optical-channel/config/frequency 1/1/1 42
./gnmi_request.sh gnmi_cli \
    get /components/component/optical-channel/config/frequency 1/1/1
```
You can find more examples in the "gnmi_request.sh" script.

### If you __don't__ have a gNMI CLI binary

Use a dedicated bazel target to build the client:
```
bazel run //tools/gnmi-go-cli:gnmi_request -- \
    [command] [yang-path] [node/port/channel]
```
For example:
```
bazel run //tools/gnmi-go-cli:gnmi_request -- \
    set-uint /components/component/optical-channel/config/frequency 1/1/1 42
bazel run //tools/gnmi-go-cli:gnmi_request -- \
    get /components/component/optical-channel/config/frequency 1/1/1
```

### Knows issues
#### Problem
If you're using the "setup_dev_env.sh" script you may face the problem with port
exposing from docker images. It means that if you run that script in two
separate terminals simultaneously expecting to run Stratum on the first and the
client CLI on the second one, you'll get two different docker containers. Hence
you should allow them both to communicate via the same port.

#### Solution
It's recommended to run the client in the same docker image. For that, you need
to run a shell on the same docker instance as Stratum's running on.

Let's assume you've already run `./setup-dev-env` command:
```
user@~/stratum-root$ ./setup-dev-env
user@a2c55385990d:/stratum$
```
Now open another terminal and find that docker image id:
```
docker ps
```
If the docker's running properly, you should see something like this:
```
user@~$ docker ps
CONTAINER ID        IMAGE           ...
907f24e2ed94        blah            ...
xxxxxxxxxxxx        stratum-dev     ...
2kldd3810283        blahblah        ...
```

Now start a shell on the proper docker image:
```
docker exec -ti xxxxxxxxxxxx bash
```

Now you can use bazel gNMI client target as described above.
