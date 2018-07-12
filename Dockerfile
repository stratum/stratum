# This image is not optimized for size, we can figure this out when Stratum
# builds

# Not supported with default docker version on Ubuntu 14.04 (1.13)
# ARG DEBIAN_TAG=jessie
# FROM debian:${DEBIAN_TAG}
FROM debian:jessie
LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="This Docker image builds the Stratum project code on a Debian distribution"

ARG BAZEL_VERSION=0.14.1

# bazel dependencies
# + wget to download bazel binary
# + ca-certificates for wget HTPPS connection
ENV PKG_DEPS pkg-config zip g++ zlib1g-dev unzip python wget ca-certificates

COPY . /stratum/
WORKDIR /stratum/

RUN apt-get update && \
    apt-get install -y --no-install-recommends $PKG_DEPS && \
    rm -rf /var/cache/apt/* /var/lib/apt/lists/*

RUN wget -q https://github.com/bazelbuild/bazel/releases/download/$BAZEL_VERSION/bazel-$BAZEL_VERSION-installer-linux-x86_64.sh && \
    chmod +x bazel-$BAZEL_VERSION-installer-linux-x86_64.sh && \
    ./bazel-$BAZEL_VERSION-installer-linux-x86_64.sh && \
    rm -f bazel-$BAZEL_VERSION-installer-linux-x86_64.sh

# TODO(antonin): build the whole code base once it is fixed, in the mean time we
# just build the P4Runtime dependency
RUN bazel build @com_github_p4lang_PI//:p4runtime_proto
# RUN bazel build ...
