# This Dockerfile expects the stratum root as its scope, hence you should build
# from root e.g.:
# docker build -t <some tag> -f stratum/p4c_backends/fpm/Dockerfile .

# We use a 2-stage build.
FROM stratumproject/build:build as builder

# Build FPM backend
COPY . /tmp/stratum
WORKDIR /tmp/stratum

RUN bazel build //stratum/p4c_backends/fpm:p4c_herc_switch

# Put aside binary and other runtime dependencies.
RUN mkdir -p /output/usr/local/bin
RUN install bazel-bin/stratum/p4c_backends/fpm/p4c_herc_switch /output/usr/local/bin/

RUN mkdir -p /output/usr/share/p4c
RUN cp -r $(bazel info output_base)/external/com_github_p4lang_p4c/p4include /output/usr/share/p4c/

# Final stage, runtime.
FROM bitnami/minideb:stretch as runtime

LABEL maintainer="Stratum dev <stratum-dev@lists.stratumproject.org>"
LABEL description="Docker-based distribution of the Stratum p4c FPM backend"

ENV RUNTIME_DEPS libgmp10 libgmpxx4ldbl gcc
RUN install_packages ${RUNTIME_DEPS}

COPY --from=builder /output /

ENTRYPOINT ["p4c_herc_switch"]
CMD ["-help"]
