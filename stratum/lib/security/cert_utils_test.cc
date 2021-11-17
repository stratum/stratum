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

TEST(cert_test, Generate_Cert) {
  BUF_MEM* mem = BUF_MEM_new();
  BUF_MEM_reserve(mem, 10000);

  // Generate keypair for CA
  EVP_PKEY* evp_ca = EVP_PKEY_new();
  EXPECT_OK(generateRSAKeyPair(evp_ca));

  // Generate self-signed CA cert
  X509* x509 = X509_new();
  EXPECT_OK(generateSignedCert(x509, evp_ca, NULL, NULL, "stratum ca", 1, 365));

  // Generate keypair for stratum cert
  EVP_PKEY* stratum_evp = EVP_PKEY_new();
  EXPECT_OK(generateRSAKeyPair(stratum_evp));

  // Generate stratum cert (signed by CA)
  X509* stratum_crt = X509_new();
  EXPECT_OK(generateSignedCert(stratum_crt, stratum_evp, x509, evp_ca,
                               "stratum", 1, 60));
}

}  // namespace stratum
