// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/security/certificate.h"

#include "openssl/bio.h"
#include "openssl/bn.h"
#include "openssl/buffer.h"
#include "openssl/evp.h"
#include "openssl/pem.h"
#include "openssl/rsa.h"
#include "stratum/lib/macros.h"

namespace stratum {

using BIGNUM_ptr = std::unique_ptr<BIGNUM, decltype(&::BN_free)>;
using BIO_ptr = std::unique_ptr<BIO, decltype(&::BIO_free)>;
using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, decltype(&::EVP_PKEY_free)>;
using X509_ptr = std::unique_ptr<X509, decltype(&::X509_free)>;

// Helper functions around OpenSSL.
namespace {
util::StatusOr<std::string> GetRSAPrivateKeyAsString(EVP_PKEY* pkey) {
  // Returns a reference to the underlying key; no need to free.
  RSA* rsa = EVP_PKEY_get0_RSA(pkey);
  CHECK_RETURN_IF_FALSE(rsa) << "Key is not an RSA key.";

  BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free);
  CHECK_RETURN_IF_FALSE(bio.get()) << "Failed to allocate string buffer.";
  CHECK_RETURN_IF_FALSE(PEM_write_bio_RSAPrivateKey(
      bio.get(), rsa, nullptr, nullptr, 0, nullptr, nullptr))
      << "Failed to write private key to buffer.";

  BUF_MEM* mem = nullptr;
  // Returns a reference to the underlying bio; no need to free.
  BIO_get_mem_ptr(bio.get(), &mem);
  if (mem != nullptr && mem->data && mem->length) {
    return std::string(mem->data, mem->length);
  }

  return MAKE_ERROR(ERR_INVALID_PARAM)
         << "Failed to write private key in PEM format.";
}

util::StatusOr<std::string> GetCertAsString(X509* x509) {
  BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free);
  CHECK_RETURN_IF_FALSE(bio.get()) << "Failed to allocate string buffer.";
  CHECK_RETURN_IF_FALSE(PEM_write_bio_X509(bio.get(), x509))
      << "Failed to write certificate to buffer.";

  BUF_MEM* mem = nullptr;
  // Returns a reference to the underlying bio; no need to free.
  BIO_get_mem_ptr(bio.get(), &mem);
  if (mem != nullptr && mem->data && mem->length) {
    return std::string(mem->data, mem->length);
  }

  return MAKE_ERROR(ERR_INVALID_PARAM)
         << "Failed to write certificate in PEM format.";
}

util::Status GenerateRSAKeyPair(EVP_PKEY* evp, int bits) {
  BIGNUM_ptr exp(BN_new(), BN_free);
  BN_set_word(exp.get(), RSA_F4);
  RSA* rsa = RSA_new();
  CHECK_RETURN_IF_FALSE(RSA_generate_key_ex(rsa, bits, exp.get(), nullptr))
      << "Failed to generate RSA key.";
  // Store this keypair in evp
  // It will be freed when EVP is freed, so only free on failure.
  if (!EVP_PKEY_assign_RSA(evp, rsa)) {
    RSA_free(rsa);
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Failed to assign key.";
  }

  return util::OkStatus();
}

util::Status GenerateUnsignedCert(X509* unsigned_cert,
                                  EVP_PKEY* unsigned_cert_key,
                                  const std::string& common_name, int serial,
                                  absl::Time valid_after,
                                  absl::Time valid_until) {
  CHECK_RETURN_IF_FALSE(
      ASN1_INTEGER_set(X509_get_serialNumber(unsigned_cert), serial));
  time_t t = absl::ToTimeT(valid_after);
  CHECK_RETURN_IF_FALSE(
      X509_time_adj_ex(X509_getm_notBefore(unsigned_cert), 0, 0, &t));
  t = absl::ToTimeT(valid_until);
  CHECK_RETURN_IF_FALSE(
      X509_time_adj_ex(X509_getm_notAfter(unsigned_cert), 0, 0, &t));
  CHECK_RETURN_IF_FALSE(X509_set_pubkey(unsigned_cert, unsigned_cert_key));
  X509_NAME* name = X509_get_subject_name(unsigned_cert);
  CHECK_RETURN_IF_FALSE(X509_NAME_add_entry_by_txt(
      name, "CN", MBSTRING_UTF8,
      reinterpret_cast<const unsigned char*>(common_name.c_str()), -1, -1, 0));

  return util::OkStatus();
}

util::Status SignCert(X509* unsigned_cert, EVP_PKEY* unsigned_cert_key,
                      X509* issuer, EVP_PKEY* issuer_key) {
  X509_NAME* name = X509_get_subject_name(unsigned_cert);
  X509_NAME* issuer_name;
  if (issuer == nullptr || issuer_key == nullptr) {
    // then self sign the cert
    issuer_name = name;
    issuer_key = unsigned_cert_key;
  } else {
    issuer_name = X509_get_subject_name(issuer);
  }
  CHECK_RETURN_IF_FALSE(X509_set_issuer_name(unsigned_cert, issuer_name));
  CHECK_RETURN_IF_FALSE(X509_sign(unsigned_cert, issuer_key, EVP_sha256()));

  return util::OkStatus();
}

util::Status GenerateSignedCert(X509* unsigned_cert,
                                EVP_PKEY* unsigned_cert_key, X509* issuer,
                                EVP_PKEY* issuer_key,
                                const std::string& common_name, int serial,
                                absl::Time valid_after,
                                absl::Time valid_until) {
  RETURN_IF_ERROR(GenerateUnsignedCert(unsigned_cert, unsigned_cert_key,
                                       common_name, serial, valid_after,
                                       valid_until));
  RETURN_IF_ERROR(
      SignCert(unsigned_cert, unsigned_cert_key, issuer, issuer_key));

  return util::OkStatus();
}
}  // namespace

Certificate::Certificate(const std::string& common_name, int serial_number)
    : key_(EVP_PKEY_ptr(EVP_PKEY_new(), EVP_PKEY_free)),
      certificate_(X509_ptr(X509_new(), X509_free)),
      common_name_(common_name),
      serial_number_(serial_number) {}

util::StatusOr<std::string> Certificate::GetPrivateKey() {
  return GetRSAPrivateKeyAsString(key_.get());
}

util::StatusOr<std::string> Certificate::GetCertificate() {
  return GetCertAsString(certificate_.get());
}

util::Status Certificate::GenerateKeyPair(int bits) {
  return GenerateRSAKeyPair(key_.get(), bits);
}

util::Status Certificate::SignCertificate(const Certificate& issuer,
                                          absl::Time valid_after,
                                          absl::Time valid_until) {
  X509* issuer_cert;
  EVP_PKEY* issuer_key;
  if (this == &issuer) {  // self sign
    issuer_cert = nullptr;
    issuer_key = nullptr;
  } else {
    issuer_cert = issuer.certificate_.get();
    issuer_key = issuer.key_.get();
  }
  return GenerateSignedCert(certificate_.get(), key_.get(), issuer_cert,
                            issuer_key, common_name_, serial_number_,
                            valid_after, valid_until);
}

}  // namespace stratum
