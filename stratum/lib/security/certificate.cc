// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/security/certificate.h"

#include "openssl/bio.h"
#include "openssl/bn.h"
#include "openssl/buffer.h"
#include "openssl/evp.h"
#include "openssl/pem.h"
#include "openssl/rsa.h"
#include "openssl/x509.h"
#include "openssl/x509v3.h"
#include "stratum/lib/macros.h"

namespace stratum {

/* TODO(bocon): Copied from a newer version of x509.h
                Remove when present in the version we use.
 */
#define X509_VERSION_3 2

// Helper functions around OpenSSL.
namespace {
util::StatusOr<std::string> GetRSAPrivateKeyAsString(EVP_PKEY* pkey) {
  // Returns a reference to the underlying key; no need to free.
  RSA* rsa = EVP_PKEY_get0_RSA(pkey);
  CHECK_RETURN_IF_FALSE(rsa) << "Key is not an RSA key.";

  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_mem()));
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

  RETURN_ERROR(ERR_INVALID_PARAM)
      << "Failed to write private key in PEM format.";
}

util::StatusOr<std::string> GetCertAsString(X509* x509) {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_mem()));
  CHECK_RETURN_IF_FALSE(bio.get()) << "Failed to allocate string buffer.";
  CHECK_RETURN_IF_FALSE(PEM_write_bio_X509(bio.get(), x509))
      << "Failed to write certificate to buffer.";

  BUF_MEM* mem = nullptr;
  // Returns a reference to the underlying bio; no need to free.
  BIO_get_mem_ptr(bio.get(), &mem);
  if (mem != nullptr && mem->data && mem->length) {
    return std::string(mem->data, mem->length);
  }

  RETURN_ERROR(ERR_INVALID_PARAM)
      << "Failed to write certificate in PEM format.";
}

util::Status GenerateRSAKeyPair(EVP_PKEY* evp, int bits) {
  bssl::UniquePtr<BIGNUM> exp(BN_new());
  BN_set_word(exp.get(), RSA_F4);
  RSA* rsa = RSA_new();
  CHECK_RETURN_IF_FALSE(RSA_generate_key_ex(rsa, bits, exp.get(), nullptr))
      << "Failed to generate RSA key.";
  // Store this keypair in evp
  // It will be freed when EVP is freed, so only free on failure.
  if (!EVP_PKEY_assign_RSA(evp, rsa)) {
    RSA_free(rsa);
    RETURN_ERROR(ERR_INVALID_PARAM) << "Failed to assign key.";
  }

  RETURN_OK();
}

util::Status GenerateUnsignedCert(X509* unsigned_cert,
                                  EVP_PKEY* unsigned_cert_key,
                                  const std::string& common_name, int serial,
                                  absl::Time valid_until) {
  CHECK_RETURN_IF_FALSE(
      ASN1_INTEGER_set(X509_get_serialNumber(unsigned_cert), serial));
  CHECK_RETURN_IF_FALSE(X509_gmtime_adj(X509_get_notBefore(unsigned_cert), 0));
  time_t t = absl::ToTimeT(valid_until);
  CHECK_RETURN_IF_FALSE(
      X509_time_adj_ex(X509_getm_notAfter(unsigned_cert), 0, 0, &t));
  CHECK_RETURN_IF_FALSE(X509_set_pubkey(unsigned_cert, unsigned_cert_key));
  X509_NAME* name = X509_get_subject_name(unsigned_cert);
  CHECK_RETURN_IF_FALSE(X509_NAME_add_entry_by_txt(
      name, "CN", MBSTRING_UTF8,
      reinterpret_cast<const unsigned char*>(common_name.c_str()), -1, -1, 0));

  RETURN_OK();
}

util::Status AddExtension(X509* cert, const X509V3_CTX& ctx,
                          const std::string& name, const std::string& value) {
  bssl::UniquePtr<X509_EXTENSION> ext(
      X509V3_EXT_nconf(nullptr, &ctx, const_cast<char*>(name.c_str()),
                       const_cast<char*>(value.c_str())));

  // Return error if the extension could not be created
  if (!ext.get()) {
    bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_mem()));
    CHECK_RETURN_IF_FALSE(bio.get()) << "Failed to allocate string buffer.";
    ERR_print_errors(bio.get());

    BUF_MEM* mem = nullptr;
    BIO_get_mem_ptr(bio.get(), &mem);
    if (mem != nullptr && mem->data && mem->length) {
      RETURN_ERROR(ERR_INVALID_PARAM).without_logging()
          << "Could not add extension " << name << " with value: " << value
          << ".\n"
          << std::string(mem->data, mem->length);
    }
  }

  // Add the extension to the end of the list
  CHECK_RETURN_IF_FALSE(X509_add_ext(cert, ext.get(), -1));

  RETURN_OK();
}

util::Status SignCert(X509* unsigned_cert, EVP_PKEY* unsigned_cert_key,
                      X509* issuer, EVP_PKEY* issuer_key) {
  bool self_signed = false;
  X509_NAME* name = X509_get_subject_name(unsigned_cert);
  X509_NAME* issuer_name;
  if (issuer == nullptr || issuer_key == nullptr) {
    // then self sign the cert
    self_signed = true;
    issuer_name = name;
    issuer_key = unsigned_cert_key;
  } else {
    issuer_name = X509_get_subject_name(issuer);
  }

  CHECK_RETURN_IF_FALSE(X509_set_version(unsigned_cert, X509_VERSION_3));
  CHECK_RETURN_IF_FALSE(X509_set_issuer_name(unsigned_cert, issuer_name));

  X509V3_CTX ctx;
  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, issuer, unsigned_cert, NULL, NULL, 0);

  std::string key_usage = "critical, digitalSignature";
  if (self_signed) key_usage += ", keyCertSign, cRLSign";
  RETURN_IF_ERROR(AddExtension(unsigned_cert, ctx, "keyUsage", key_usage));

  std::string is_ca = "critical, CA:";
  is_ca += self_signed ? "TRUE" : "FALSE";
  RETURN_IF_ERROR(AddExtension(unsigned_cert, ctx, "basicConstraints", is_ca));

  RETURN_IF_ERROR(
      AddExtension(unsigned_cert, ctx, "subjectKeyIdentifier", "hash"));

  if (!self_signed) {
    RETURN_IF_ERROR(
        AddExtension(unsigned_cert, ctx, "extendedKeyUsage", "serverAuth"));
    RETURN_IF_ERROR(
        AddExtension(unsigned_cert, ctx, "authorityKeyIdentifier", "keyid"));
  }

  CHECK_RETURN_IF_FALSE(X509_sign(unsigned_cert, issuer_key, EVP_sha256()));

  RETURN_OK();
}

util::Status GenerateSignedCert(X509* unsigned_cert,
                                EVP_PKEY* unsigned_cert_key, X509* issuer,
                                EVP_PKEY* issuer_key,
                                const std::string& common_name, int serial,
                                absl::Time valid_until) {
  RETURN_IF_ERROR(GenerateUnsignedCert(unsigned_cert, unsigned_cert_key,
                                       common_name, serial, valid_until));
  RETURN_IF_ERROR(
      SignCert(unsigned_cert, unsigned_cert_key, issuer, issuer_key));

  RETURN_OK();
}
}  // namespace

Certificate::Certificate(const std::string& common_name, int serial_number)
    : key_(bssl::UniquePtr<EVP_PKEY>(EVP_PKEY_new())),
      certificate_(bssl::UniquePtr<X509>(X509_new())),
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
                            valid_until);
}

}  // namespace stratum
