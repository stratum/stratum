#!/bin/bash
# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
set -e
THIS_DIR=$(dirname "${BASH_SOURCE[0]}")
COMMON_NAME=${COMMON_NAME:-"stratum.local"}

mkdir -p "$THIS_DIR/certs"
rm -rf "$THIS_DIR/certs/*"

SERVER_CONF_FILE="$(mktemp)"

echo "[ req ]
default_bits           = 2048
distinguished_name     = stratum
prompt                 = no
req_extensions         = req_ext

[ stratum ]
C                      = US
ST                     = CA
L                      = Menlo Park
O                      = Open Networking Foundation
OU                     = Stratum
CN                     = $COMMON_NAME

[ alternate_names ]
DNS.1                  = $COMMON_NAME

[ req_ext ]
subjectAltName         = @alternate_names
" > "$SERVER_CONF_FILE"

# Generate Private keys and certificates for CA
openssl genrsa -out "$THIS_DIR/certs/ca.key" 2048
openssl req -x509 -nodes -new -config "$THIS_DIR/ca.conf" -key "$THIS_DIR/certs/ca.key" \
                  -days 365 -out "$THIS_DIR/certs/ca.crt"

# Generate Private keys and certificate signing request(CSR) for Stratum gRPC server
openssl genrsa -out "$THIS_DIR/certs/stratum.key" 2048
openssl req -new -config "$SERVER_CONF_FILE" -key "$THIS_DIR/certs/stratum.key" -out "$THIS_DIR/certs/stratum.csr"

# Sing a certificate for Stratum with CA
openssl x509 -req -in "$THIS_DIR/certs/stratum.csr" -CA "$THIS_DIR/certs/ca.crt" \
             -CAkey "$THIS_DIR/certs/ca.key" -CAcreateserial \
             -out "$THIS_DIR/certs/stratum.crt" -days 30 \
             -extensions req_ext -extfile "$SERVER_CONF_FILE"

# (Optional) Generate Private keys and certificate for gRPC client
openssl genrsa -out "$THIS_DIR/certs/client.key" 2048
openssl req -new -config "$THIS_DIR/grpc-client.conf" -key "$THIS_DIR/certs/client.key" -out "$THIS_DIR/certs/client.csr"
openssl x509 -req -in "$THIS_DIR/certs/client.csr" -CA "$THIS_DIR/certs/ca.crt" \
             -CAkey "$THIS_DIR/certs/ca.key" -CAcreateserial \
             -out "$THIS_DIR/certs/client.crt" -days 30

# Cleanup
rm "$SERVER_CONF_FILE"
