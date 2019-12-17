#!/bin/bash

################################################################################
# README.
################################################################################

# Build the dummy switch.
# bazel build //stratum/hal/bin/dummy:stratum_dummy
#
# Run Stratum dummy switch.
# ./bazel-bin/stratum/hal/bin/dummy/stratum_dummy \
#   --external_stratum_urls=0.0.0.0:28000 \
#   --chassis_config_file="/stratum/stratum/hal/bin/dummy/chassis_config" \
#   --persistent_config_dir=/tmp/
#
# Put the gNMI CLI client binary and name it as 'gnmi-go-cli'.
#
# Use this script to get/set value(s) under the target path. Examples:
#
# ./gnmi_request.sh gnmi_cli \
#   get \
#   /components/component/optical-channel/state/frequency \
#   1/1/1
#
# ./gnmi_request.sh gnmi_cli \
#   set-uint \
#   /components/component/optical-channel/config/frequency \
#   1/1/1 \
#   100500
#
# ./gnmi_request.sh gnmi_cli \
#   set-decimal \
#   /components/component/optical-channel/config/output-power \
#   1/1/1 \
#   6089 2
#
# ./gnmi_request.sh gnmi_cli \
#   subscribe-sample \
#   /components/component/optical-channel/state/input-power \
#   1/1/1 \
#
# ./gnmi_request.sh gnmi_cli \
#   subscribe-change \
#   /components/component/optical-channel/state/frequency \
#   1/1/1 \
#


################################################################################
# Constants definition.
################################################################################

GET_CMD="get"

SET_CMD="set"
SET_UINT_CMD="set-uint"
SET_INT_CMD="set-int"
SET_STRING_CMD="set-string"
SET_DECIMAL_CMD="set-decimal"

SUBSCRIBE_CMD="subscribe"
SUBSCRIBE_ON_CHANGE_CMD="subscribe-change"
SUBSCRIBE_SAMPLE_CMD="subscribe-sample"

CAPABILITIES_CMD="capabilities"

# Subscription interval in milliseconds.
SUBSCRIPTION_INTERVAL=1000


################################################################################
# Input arguments declaration.
################################################################################

PATH_TO_CLI=$1

# Command type parameter.
CMD_TYPE=$2

# If "capabilities" then the other args should't be specified.
if [ "$CMD_TYPE" != *"$CAPABILITIES_CMD" ]
then
  # Path parameter.
  PATH_RAW=$3
  # Target component or interface name.
  COMPONENT_NAME=$4
  # First set parameter. Used for most cases as the only set value.
  if [[ "$CMD_TYPE" == *"$SET_CMD"* ]]
  then
    SET_VALUE_1=$5
  fi

  # Second set parameter. Used for decimal value as 'precision' field while
  # first parameter is 'decimal' field.
  if [[ "$CMD_TYPE" == "$SET_DECIMAL_CMD" ]]
  then
    SET_VALUE_2=$6
  fi
fi


################################################################################
# Global variables declaration.
################################################################################

REQUEST_STR=""
PATH_STR=""


################################################################################
# Functions definition.
################################################################################

# Build proto-formatted to the YANG node.
function path_to_str {
  # Split string by slash.
  IFS='/'
  read -ra SPLITTED <<< "$PATH_RAW"
  # Append node by node to the path.
  PATH_STR=$'path:{'
  for ARG in "${SPLITTED[@]}"; do
    # First arg can be empty because the path usually starts from slash.
    if [ "$ARG" == "" ]
    then continue;
    fi
    TMP=""
    # If the node is 'component' node then component name should be added.
    if [ "$ARG" == "component" ]
    then TMP=$(cat <<EOF
elem:{
  name: '$ARG'
  key:{
    key: 'name'
    value: '$COMPONENT_NAME'
  }
}
EOF
    )
    # If the node is 'interface' node then interface name should be added.
    elif [ "$ARG" == "interface" ]
    then TMP=$(cat <<EOF
elem:{
  name: '$ARG'
  key:{
    key: 'name'
    value: '$COMPONENT_NAME'
  }
}
EOF
    )
    # Simple node.
    else
      TMP=" elem:{ name: '$ARG' }"
    fi
    # Assign current node to path.
    PATH_STR="$PATH_STR"$'\n'"$TMP";
  done
  # Save result to global variable.
  PATH_STR="$PATH_STR"$'\n'"}"
}

# Create a proto request for PATH_STR.
function create_request {
  # Add 'Get' request header and footer to the path.
  if [ "$CMD_TYPE" == "$GET_CMD" ]
  then
    REQUEST_STR="encoding:PROTO"$'\n'
    REQUEST_STR="$REQUEST_STR$PATH_STR"$'\n'
  # Add 'Set' request header and footer to the path.
  elif [[ $CMD_TYPE == *"$SET_CMD"* ]]
  then
    # Add update section
    REQUEST_STR="update:{"$'\n'
    # Add path as first update section argument.
    REQUEST_STR="$REQUEST_STR$PATH_STR"$'\n'
    # Add value as second update section argument.
    REQUEST_STR="$REQUEST_STR""val:{"$'\n'
    # Add uint value.
    if [ $CMD_TYPE == "$SET_UINT_CMD" ]
    then REQUEST_STR="$REQUEST_STR"" uint_val: $SET_VALUE_1"$'\n'
    # Add int value.
    elif [ $CMD_TYPE == "$SET_INT_CMD" ]
    then REQUEST_STR="$REQUEST_STR"" int_val: $SET_VALUE_1"$'\n'
    # Add string value (should be surrounded by '\'' symbols).
    elif [ $CMD_TYPE == "$SET_STRING_CMD" ]
    then REQUEST_STR="$REQUEST_STR"" string_val: '$SET_VALUE_1'"$'\n'
    # Add decimal value as pair of 'decimal' and 'precision'.
    elif [ $CMD_TYPE == "$SET_DECIMAL_CMD" ]
    then
      REQUEST_STR="$REQUEST_STR decimal_val:{"$'\n'
      REQUEST_STR="$REQUEST_STR""  digits: $SET_VALUE_1"$'\n'
      REQUEST_STR="$REQUEST_STR""  precision: $SET_VALUE_2"$'\n'
      REQUEST_STR="$REQUEST_STR"" }"$'\n'
    fi
    # Close update and value sections.
    REQUEST_STR="$REQUEST_STR""}"$'\n'
    REQUEST_STR="$REQUEST_STR""}"$'\n'
  # Add 'Subscribe' request header and footer to the path.
  elif [[ $CMD_TYPE == *"$SUBSCRIBE_CMD"* ]]
  then
    # Add subsribe list.
    REQUEST_STR="subscribe{"$'\n'
    # Set stream mode by default.
    REQUEST_STR="$REQUEST_STR"" mode: STREAM"$'\n'
    # Add default prefix.
    REQUEST_STR="$REQUEST_STR"" prefix: {}"$'\n'
    # Add subscription instance.
    REQUEST_STR="$REQUEST_STR"" subscription {"$'\n'
    # Add the path to subscribe to.
    REQUEST_STR="$REQUEST_STR$PATH_STR"$'\n'
    # Choose subscription type.
    if [ $CMD_TYPE == "$SUBSCRIBE_ON_CHANGE_CMD" ]
      then REQUEST_STR="$REQUEST_STR"" mode: ON_CHANGE"$'\n'
    elif [ $CMD_TYPE == "$SUBSCRIBE_SAMPLE_CMD" ]
      then REQUEST_STR="$REQUEST_STR"" mode: SAMPLE"$'\n'
    fi
    # If sampple subscription set sample interval.
    if [ $CMD_TYPE == "$SUBSCRIBE_SAMPLE_CMD" ]
    then
      REQUEST_STR="$REQUEST_STR"" sample_interval: $SUBSCRIPTION_INTERVAL"$'\n'
    fi
    # Close subscription and subsribtion list.
    REQUEST_STR="$REQUEST_STR""}"$'\n'
    REQUEST_STR="$REQUEST_STR""}"$'\n'
  fi
}

# Process the REQUEST_STR.
function process_cmd {
  if [ "$CMD_TYPE" == "$GET_CMD" ]
  then "$PATH_TO_CLI" -address 0.0.0.0:28000 -get -proto "$REQUEST_STR"
  elif [[ $CMD_TYPE == *"$SET_CMD"* ]]
  then "$PATH_TO_CLI" -address 0.0.0.0:28000 -set -proto "$REQUEST_STR"
  elif [[ $CMD_TYPE == *"$SUBSCRIBE_CMD"* ]]
  then "$PATH_TO_CLI" -address 0.0.0.0:28000 -dt "single" -proto "$REQUEST_STR"
  elif [ $CMD_TYPE == "$CAPABILITIES_CMD" ]
  then "$PATH_TO_CLI" -address 0.0.0.0:28000 -capabilities -proto ""
  else echo "Unsupported command."
  fi
}


################################################################################
# Main. Entry point.
################################################################################

# Build path string.
path_to_str

# Build proto request.
create_request

# Uncomment the next line to see the proto request.
# echo "$REQUEST_STR"

# Process command.
process_cmd
