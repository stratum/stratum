// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/security/cert_utils.h"

#include <iostream>

#include "gtest/gtest.h"
#include "openssl/bio.h"
#include "openssl/pem.h"
#include "stratum/glue/status/status_test_util.h"

namespace stratum {
namespace {

// Helper method to print an X509 cert.
void printCert(X509* cert, BUF_MEM* mem) {
  BIO* bio = BIO_new(BIO_s_mem());
  PEM_write_bio_X509(bio, cert);
  BIO_get_mem_ptr(bio, &mem);
  std::string str;
  if (mem->data && mem->length) {
    str.assign(mem->data, mem->length);
  }

  std::cout << str << std::endl;
  BIO_free(bio);
}

}  // namespace

using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, decltype(&::EVP_PKEY_free)>;
using X509_ptr = std::unique_ptr<X509, decltype(&::X509_free)>;


TEST(cert_test, Generate_Cert) {
  // BUF_MEM* mem = BUF_MEM_new();
  // BUF_MEM_reserve(mem, 10000);

  // Generate keypair for CA
  EVP_PKEY_ptr ca_key(EVP_PKEY_new(), EVP_PKEY_free);
  EXPECT_OK(generateRSAKeyPair(ca_key.get()));

  // Generate self-signed CA cert
  X509_ptr ca_cert(X509_new(), X509_free);
  EXPECT_OK(generateSignedCert(ca_cert.get(), ca_key.get(), NULL, NULL, "stratum ca", 1, 365));

  // Generate keypair for stratum cert
  EVP_PKEY_ptr stratum_key(EVP_PKEY_new(), EVP_PKEY_free);
  EXPECT_OK(generateRSAKeyPair(stratum_key.get()));

  // Generate stratum cert (signed by CA)
  X509_ptr stratum_cert(X509_new(), X509_free);
  EXPECT_OK(generateSignedCert(stratum_cert.get(), stratum_key.get(), ca_cert.get(), ca_key.get(),
                               "stratum", 1, 60));
}

}  // namespace stratum
