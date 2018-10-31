#!/bin/bash

# This script:
# - gets the latest version of `ygot` - the YANG-to-PROTO converter.
# - compiles the tool
# - runs the converter with all necessary command-line options
# - jumps into the //platforms/networking/hercules/public/proto/openconfig
#   directory and creates BUILD files
# - calls a BUILD-file-beautifier

# The script has to be run from //platforms/networking/hercules/public/yang
# directory.
# It does not expect any command-line parameters.

# Step 1: Download the latest source of the generator and build it.
mkdir /tmp/gopath
export GOPATH=/tmp/gopath
go get -d -u github.com/openconfig/ygot
pushd ${GOPATH}/src/github.com/openconfig/ygot
go get ./...

# Step 2: build `ygot`
pushd proto_generator
go build

# Step 3: Use the generator to generate PROTO files.
popd
popd
export OUTPUT_PATH=../proto
export HPATH=.
export GENERATOR=/tmp/gopath/src/github.com/openconfig/ygot/proto_generator/proto_generator
${GENERATOR} -output_dir=${OUTPUT_PATH} \
  -base_import_path=platforms/networking/hercules/public/proto \
  -exclude_modules=ietf-interfaces \
  -path=platforms/networking/hercules/public/proto \
  -ywrapper_path=platforms/networking/hercules/public/proto \
  -yext_path=platforms/networking/hercules/public/proto \
  ${HPATH}/*.yang

# Generate sources (*.proto).
function sources {
  for file in $(ls *.proto)
  do
    if [[ "${file}" != "enums.proto" ]]
    then
      echo "        \"${file}\"," >> BUILD
    fi
  done
}

# Generate dependencies (sub-directories).
function dependencies {
  # Special handling of enums.proto - it is needed by target in sub-directories.
  if [[ -e enums.proto ]]
  then
    echo "" > /dev/null
  else
  cat <<EOT >>BUILD
        "//platforms/networking/hercules/public/proto/openconfig:${3}enums_proto",
EOT
  fi
  for dir in $(find .  -maxdepth 1 -type d  | sed 's:^\./::' | grep -v "\.")
  do
    echo "        \"${2}/${1}/${dir}:${3}openconfig_proto\"," >> BUILD
  done
}

# Generate sc_proto_lib statement.
function sc_proto_lib {
  cat <<EOT >>BUILD
sc_proto_lib(
    name = "sc_openconfig_proto",
    hdrs = [
EOT
  sources
  cat <<EOT >>BUILD
    ],
    deps = [
        "//platforms/networking/hercules/public/proto:sc_yang_proto",
EOT
  dependencies "${1}" "${2}" "${3}"
  cat <<EOT >>BUILD
    ],
)

EOT
  if [[ -e enums.proto ]]
  then
  cat <<EOT >>BUILD
sc_proto_lib(
    name = "sc_enums_proto",
    hdrs = [
        "enums.proto",
    ],
    deps = [
        "//platforms/networking/hercules/public/proto:sc_yang_proto",
    ],
)

EOT
  fi
}

# Generate cc_proto_lib statement.
function cc_proto_lib {
  cat <<EOT >>BUILD
proto_library(
    name = "openconfig_proto",
    srcs = [
EOT
  sources
  cat <<EOT >>BUILD
    ],
    #cc_api_version = 2, FIXME(boc) google only
    deps = [
        "//platforms/networking/hercules/public/proto:yang_proto",
EOT
  dependencies "${1}" "${2}" "${3}"
  cat <<EOT >>BUILD
    ],
)

cc_proto_library(
    name = "openconfig_cc_proto",
    deps = [
        ":openconfig_proto",
    ],
)

EOT
  if [[ -e enums.proto ]]
  then
  cat <<EOT >>BUILD
proto_library(
    name = "enums_proto",
    srcs = [
        "enums.proto",
    ],
    #cc_api_version = 2, FIXME(boc) google only
    deps = [
        "//platforms/networking/hercules/public/proto:yang_proto",
    ],
)

cc_proto_library(
    name = "enums_cc_proto",
    deps = [
        ":enums_proto",
    ],
)

EOT
  fi
}

# Generate the contents of the BUILD file.
function create_BUILD {
  pushd "${1}" > /dev/null
  echo "Generating BUILD in: ${2}/${1}"
  cat <<EOT >BUILD
load(
    "//platforms/networking/sandblaze/portage:build_defs.bzl",
    "STRATUM_INTERNAL",
    "sc_proto_lib",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

EOT
  sc_proto_lib "${1}" "${2}" "sc_"

  # Uncomment the following line to add proto_library definitions.
  # cc_proto_lib "${1}" "${2}" ""

  # Done with the current directory. Process sub-directores.
  for dir in $(find .  -maxdepth 1 -type d  | sed 's:^\./::' | grep -v "\.")
  do
    create_BUILD "${dir}" "${2}/${1}"
  done
  popd > /dev/null
}

# Step 4: Generate BUILD files for all sub-directories.
# pushd ../proto
# create_BUILD "openconfig" "//platforms/networking/hercules/public/proto"

# Step 5: Fix the BUILD files.
# buildifier -v "$(find openconfig -name BUILD)"
