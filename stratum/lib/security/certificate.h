// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_SECURITY_CERTIFICATE_H_
#define STRATUM_LIB_SECURITY_CERTIFICATE_H_

#include <memory>
#include <string>

#include "absl/time/time.h"
#include "openssl/bio.h"
#include "openssl/bn.h"
#include "openssl/evp.h"
#include "openssl/x509.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/utils.h"

namespace stratum {

// The Certificate class encapsulates common tasks around x509 certificates.
// TODO(bocon): set CA attribute in X509v3
class Certificate {
 public:
  Certificate(const std::string& common_name, int serial_number);

  // Returns PEM-encoded representation of the private key.
  util::StatusOr<std::string> GetPrivateKey();

  // Returns PEM-encoded representation of the X509 certificate.
  util::StatusOr<std::string> GetCertificate();

  // Generate RSA key pair with key length as specified.
  util::Status GenerateKeyPair(int bits);

  // Signs a certificate using the provided certificate.
  util::Status SignCertificate(const Certificate& issuer,
                               absl::Time valid_after, absl::Time valid_until);

 private:
  std::unique_ptr<EVP_PKEY, decltype(&::EVP_PKEY_free)> key_;
  std::unique_ptr<X509, decltype(&::X509_free)> certificate_;
  std::string common_name_;
  int serial_number_;
};

}  // namespace stratum

#endif  // STRATUM_LIB_SECURITY_CERTIFICATE_H_
