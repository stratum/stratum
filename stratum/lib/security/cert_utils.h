// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

#include "openssl/evp.h"
#include "openssl/x509.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/utils.h"

namespace stratum {

util::StatusOr<std::string> GetRSAPrivateKeyAsString(EVP_PKEY* pkey);
util::StatusOr<std::string> GetCertAsString(X509* x509);

util::Status generateRSAKeyPair(EVP_PKEY* pkey, int bits = 1024);
util::Status generateSignedCert(X509* unsigned_cert,
                                EVP_PKEY* unsigned_cert_key, X509* issuer,
                                EVP_PKEY* issuer_key,
                                const std::string& common_name, int serial,
                                int days);
util::Status generateUnsignedCert(X509* unsigned_cert,
                                  EVP_PKEY* unsigned_cert_key,
                                  const std::string& common_name, int serial,
                                  int days);
util::Status signCert(X509* unsigned_cert, EVP_PKEY* unsigned_cert_key,
                      X509* issuer, EVP_PKEY* issuer_key,
                      const std::string& common_name, int serial, int days);


class Certificate {
public:
  Certificate(const std::string& certificate, const std::string& private_key);

  // Returns PEM-encoded representation of the private key
  virtual util::StatusOr<std::string> GetPrivateKey();
  // Returns PEM-encoded representation of the X509 certificate
  virtual util::StatusOr<std::string> GetCertificate();


  // virtual util::Status SignCertificate(Certificate& certificate) = 0;
  // virtual util::Status IsSelfSigned() = 0;
  // virtual util::Status Verify(const Certificate& root_certificate) = 0;

  static Certificate GenerateCertificate(const std::string& common_name, int serial_number, int bits);

protected:

  Certificate();

  std::string private_key_string_;
  std::string certificate_string_;
};

}  // namespace stratum

// todo: seed random number generator before using this in production code
// todo: set CA attribute in X509v3