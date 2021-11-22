// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/security/cert_utils.h"

#include "gtest/gtest.h"
#include "openssl/bio.h"
#include "openssl/pem.h"
#include "stratum/glue/status/status_test_util.h"

namespace stratum {

using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, decltype(&::EVP_PKEY_free)>;
using X509_ptr = std::unique_ptr<X509, decltype(&::X509_free)>;

TEST(cert_test, Generate_Cert) {
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


TEST(cert_test, Generate_Cert2) {
  Certificate cert = std::move(Certificate::GenerateCertificate("stratum ca", 1, 1024));
  cert.GetPrivateKey();
  cert.GetCertificate();
}

}  // namespace stratum
