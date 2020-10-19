# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# This Dockerfile expects the stratum root as its scope, hence you should build
# from root e.g.:
# docker build -t <some tag> -f tools/mininet/Dockerfile .

# We use a 2-stage build. Build all tools first, then copy only the strict necessary
# to a new image with runtime dependencies.
FROM stratumproject/build:build as builder

ENV BUILD_DEPS \
    python-setuptools \
    python-pip \
    help2man
RUN apt-get update && \
    apt-get install -y --no-install-recommends ${BUILD_DEPS}

# Build Mininet
RUN mkdir /tmp/mininet
WORKDIR /tmp/mininet
RUN curl -L https://github.com/mininet/mininet/tarball/master | \
    tar xz --strip-components 1
# Install in a special directory that we will copy to the runtime image.
RUN mkdir -p /output
RUN PREFIX=/output make install-mnexec install-manpages
RUN python setup.py install --root /output
# Install `m` utility so user can attach to a mininet host directly
RUN cp util/m /output/bin/m && sed -i 's#sudo##g' /output/bin/m

# Build stratum_bmv2
COPY . /tmp/stratum
WORKDIR /tmp/stratum

RUN bazel build //stratum/hal/bin/bmv2:stratum_bmv2

# Put aside stratum_bmv2 binary and shared objects needed for runtime.
RUN mkdir -p /output/usr/local/bin
RUN cp ./bazel-bin/stratum/hal/bin/bmv2/stratum_bmv2 /output/usr/local/bin
RUN cp --parents /usr/local/lib/libsimpleswitch_runner.so.0 /output
RUN cp --parents /usr/local/lib/libbmpi.so.0 /output

# As well as the P4Runtime Python bindings installed by PI. This is not needed
# to run mininet, but it's useful to execute Python scripts acting as a
# P4Runtime client, e.g. control plane apps or PTF tests.
RUN bazel build @com_github_p4lang_p4runtime//:p4runtime_proto \
    @com_google_protobuf//:protobuf_python \
    @com_google_googleapis//google/rpc/... \
    @com_github_grpc_grpc//src/compiler:grpc_python_plugin
ENV PYTHON_PACKAGE_BASE /output/usr/local/lib/python2.7/dist-packages
RUN ./bazel-out/host/bin/external/com_google_protobuf/protoc \
    ./bazel-stratum/external/com_github_p4lang_p4runtime/p4/v1/p4data.proto \
    ./bazel-stratum/external/com_github_p4lang_p4runtime/p4/v1/p4runtime.proto \
    ./bazel-stratum/external/com_github_p4lang_p4runtime/p4/config/v1/p4info.proto \
    ./bazel-stratum/external/com_github_p4lang_p4runtime/p4/config/v1/p4types.proto \
    ./bazel-stratum/external/com_google_googleapis/google/rpc/status.proto \
    ./bazel-stratum/external/com_google_googleapis/google/rpc/code.proto \
    -I./bazel-stratum/external/com_google_googleapis -I./bazel-stratum/external/com_github_p4lang_p4runtime \
    -I./bazel-stratum/external/com_google_protobuf/src \
    --python_out $PYTHON_PACKAGE_BASE --grpc_out $PYTHON_PACKAGE_BASE  \
    --plugin=protoc-gen-grpc=./bazel-bin/external/com_github_grpc_grpc/src/compiler/grpc_python_plugin
RUN touch $PYTHON_PACKAGE_BASE/p4/__init__.py && \
    touch $PYTHON_PACKAGE_BASE/p4/v1/__init__.py && \
    touch $PYTHON_PACKAGE_BASE/p4/config/__init__.py && \
    touch $PYTHON_PACKAGE_BASE/p4/config/v1/__init__.py \
    touch $PYTHON_PACKAGE_BASE/google/__init__.py && \
    touch $PYTHON_PACKAGE_BASE/google/rpc/__init__.py

# Install a version of the protobuf and grpc python bindings that is
# compatible to that used to generate the P4Runtime ones.
ARG PROTOBUF_VER=3.13.0
ARG GRPC_VER=1.30.0
RUN pip install --root /output "protobuf<=${PROTOBUF_VER}"
RUN pip install --root /output "grpcio<=${GRPC_VER}"

# Final stage, runtime.
FROM bitnami/minideb:stretch as runtime

LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="Docker-based Mininet image that uses stratum_bmv2 as the default switch"

# Mininet and BMv2 simple_switch runtime dependencies
ENV RUNTIME_DEPS \
    iproute2 \
    iputils-ping \
    net-tools \
    ethtool \
    socat \
    psmisc \
    procps \
    iperf \
    arping \
    telnet \
    python-pexpect \
    tcpdump \
    screen \
    libboost-filesystem1.62.0 \
    libboost-program-options1.62.0 \
    libboost-thread1.62.0 \
    libjudydebian1 \
    libgmp10 \
    libpcap0.8
RUN install_packages $RUNTIME_DEPS

COPY --from=builder /output /
RUN ldconfig

WORKDIR /root
COPY stratum/hal/bin/bmv2/dummy.json ./dummy.json
COPY tools/mininet/stratum.py ./stratum.py
ENV PYTHONPATH $PYTHONPATH:/root

# We need to expose one port per stratum_bmv2 instance, hence the number of
# exposed ports limit the number of switches that can be controlled from an
# external P4Runtime controller.
EXPOSE 50001-50100

ENTRYPOINT ["mn", "--custom", "/root/stratum.py", "--switch", "stratum-bmv2", "--host", "no-offload-host", "--controller", "none"]
