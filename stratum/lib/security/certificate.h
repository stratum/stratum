// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_SECURITY_CERTIFICATE_H_
#define STRATUM_LIB_SECURITY_CERTIFICATE_H_

#include <memory>
#include <string>

#include "absl/time/time.h"
#include "openssl/evp.h"
#include "openssl/x509.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/utils.h"

namespace stratum {

// The Certificate class encapsulates common tasks around X509 certificates.
class Certificate {
 public:
  // Creates a new Certificate with the given common name (CN).
  explicit Certificate(const std::string& common_name);

  // Returns PEM-encoded representation of the private key.
  util::StatusOr<std::string> GetPrivateKey();

  // Returns PEM-encoded representation of the X509 certificate.
  util::StatusOr<std::string> GetCertificate();

  // Generates a RSA key pair with key length as specified.
  util::Status GenerateKeyPair(int bits = 4096);

  // Loads the certificate and private key from strings.
  util::Status LoadCertificate(const std::string& cert_pem,
                               const std::string& key_pem);

  // Returns true if the certificate is a CA certificate.
  util::StatusOr<bool> IsCA();

  // Checks that the certificate common name matches the provided string.
  util::Status CheckCommonName(const std::string& name);

  // Returns the common name of the certificate.
  const std::string& GetCommonName();

  // Check that the issuer of the certificate matches the provided certificate.
  util::Status CheckIssuer(const Certificate& issuer);

  // Returns the serial number of the certificate as a hex string.
  util::StatusOr<std::string> GetSerialNumber();

  // Signs a certificate using the provided certificate.
  util::Status SignCertificate(const Certificate& issuer,
                               absl::Time valid_after, absl::Time valid_until,
                               int serial = 0);

 private:
  bssl::UniquePtr<EVP_PKEY> key_;
  bssl::UniquePtr<X509> certificate_;
  std::string common_name_;
};

}  // namespace stratum

#endif  // STRATUM_LIB_SECURITY_CERTIFICATE_H_
