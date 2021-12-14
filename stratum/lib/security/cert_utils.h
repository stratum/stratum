// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

#include "openssl/bio.h"
#include "openssl/bn.h"
#include "openssl/evp.h"
#include "openssl/x509.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/utils.h"

namespace stratum {

using BIGNUM_ptr = std::unique_ptr<BIGNUM, decltype(&::BN_free)>;
using BIO_ptr = std::unique_ptr<BIO, decltype(&::BIO_free)>;
using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, decltype(&::EVP_PKEY_free)>;
using X509_ptr = std::unique_ptr<X509, decltype(&::X509_free)>;

util::StatusOr<std::string> GetRSAPrivateKeyAsString(EVP_PKEY* pkey);
util::StatusOr<std::string> GetCertAsString(X509* x509);
util::Status GenerateRSAKeyPair(EVP_PKEY* pkey, int bits = 1024);
util::Status GenerateUnsignedCert(X509* unsigned_cert,
                                  EVP_PKEY* unsigned_cert_key,
                                  const std::string& common_name, int serial,
                                  int days);
util::Status SignCert(X509* unsigned_cert, EVP_PKEY* unsigned_cert_key,
                      X509* issuer, EVP_PKEY* issuer_key,
                      const std::string& common_name, int serial, int days);
util::Status GenerateSignedCert(X509* unsigned_cert,
                                EVP_PKEY* unsigned_cert_key, X509* issuer,
                                EVP_PKEY* issuer_key,
                                const std::string& common_name, int serial,
                                int days);

class Certificate {
 public:
  Certificate(const std::string& common_name, int serial_number);

  // Returns PEM-encoded representation of the private key
  util::StatusOr<std::string> GetPrivateKey();

  // Returns PEM-encoded representation of the X509 certificate
  util::StatusOr<std::string> GetCertificate();

  // Generate RSA key pair with key length as specified
  util::Status GenerateKeyPair(int bits);

  // Signs a certificate using the provided certificate
  util::Status SignCertificate(Certificate& issuer, int days_valid);

 private:
  EVP_PKEY_ptr key_;
  X509_ptr certificate_;

  std::string common_name_;
  int serial_number_;
};

}  // namespace stratum

// TODO(bocon): seed random number generator before using this in production code
// TODO(bocon): set CA attribute in X509v3
