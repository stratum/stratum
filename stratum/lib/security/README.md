<!--
Copyright 2020-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->

# Stratum security components

## Authentication policy checker

> This component is currently work in progress.

## Credentials manager

The credentials manager manages the server credentials for external facing gRPC servers. It generates `InsecureServerCredentials` to the HAL component if no credential is given (Certificate and Private key)

If credentials are provided and valid, the credentials manager will provide a `TlsServerCredentials` to the HAL component so it can use this credential config to start the gRPC server.

## Enable SSL/TLS support

To start Stratum with SSL/TLS, you need to provide credential below:

 - CA certificate
 - Server certificate
 - Server private key

According to the gRPC [document][1], the certificate and private key need to be specified in PEM format.

You can use tools like [OpenSSL][2] to generate these files. We also provide a [script][3] to create credentials for both the server-side and client-side.

To start Stratum with SSL/TLS, add the following flags:
```
--ca_cert_file=[CA certificate file]
--server_cert_file=[Server certificate file]
--server_key_file=[Server private key file]
```

[1]:https://grpc.io/docs/guides/auth/#with-server-authentication-ssltls-5
[2]:https://www.openssl.org/
[3]:../../../tools/tls/generate-certs.sh
