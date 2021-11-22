// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

#include "openssl/evp.h"
#include "openssl/x509.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/utils.h"

namespace stratum {

util::StatusOr<std::string> getRSAPrivateKeyAsString(EVP_PKEY* pkey);
util::StatusOr<std::string> getCertAsString(X509* x509);

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

  virtual util::StatusOr<std::string> GetPrivateKey() = 0;
  virtual util::StatusOr<std::string> GetX509Certificate() = 0;
  // virtual util::Status SignCertificate(Certificate& certificate) = 0;
  // virtual util::Status IsSelfSigned() = 0;
  // virtual util::Status Verify(const Certificate& root_certificate) = 0;

  static util::StatusOr<Certificate> GenerateCertificate(const std::string& common_name, int serial_number);
  static util::StatusOr<Certificate> GetCertificate(const std::string& certificate, const std::string& private_key = "");


protected:
  Certificate() = delete;
  Certificate(const std::string& common_name, int serial_number);
  Certificate(const std::string& certificate, const std::string& private_key = "");

  std::string private_key_;
  std::string certificate_;
  std::string common_name_;
  int serial_number_;
  bool initialized_;
};

}  // namespace stratum

// todo: seed random number generator before using this in production code
// todo: set CA attribute in X509v3