// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/security/cert_utils.h"

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

util::StatusOr<std::string> GetRSAPrivateKeyAsString(EVP_PKEY* pkey) {
  // Returns a reference to the underlying key; no need to free.
  RSA* rsa = EVP_PKEY_get0_RSA(pkey);
  CHECK_RETURN_IF_FALSE(rsa) << "Key is not an RSA key.";

  BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free);
  CHECK_RETURN_IF_FALSE(bio.get()) << "Failed to allocate string buffer.";
  CHECK_RETURN_IF_FALSE(PEM_write_bio_RSAPrivateKey(bio.get(), rsa, NULL,
                                                    NULL, 0, NULL, NULL)) << "Failed to write private key to buffer.";

  BUF_MEM* mem = nullptr;
  // Returns a reference to the underlying bio; no need to free.
  BIO_get_mem_ptr(bio.get(), &mem);
  if (mem != nullptr && mem->data && mem->length) {
    return std::string(mem->data, mem->length);
  }
  RETURN_ERROR(ERR_INVALID_PARAM) << "Failed to write private key in PEM format.";
}

util::StatusOr<std::string> GetCertAsString(X509* x509) {
  BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free);
  CHECK_RETURN_IF_FALSE(bio.get()) << "Failed to allocate string buffer.";
  CHECK_RETURN_IF_FALSE(PEM_write_bio_X509(bio.get(), x509)) << "Failed to write certificate to buffer.";

  BUF_MEM* mem = nullptr;
  // Returns a reference to the underlying bio; no need to free.
  BIO_get_mem_ptr(bio.get(), &mem);
  if (mem != nullptr && mem->data && mem->length) {
    return std::string(mem->data, mem->length);
  }
  RETURN_ERROR(ERR_INVALID_PARAM) << "Failed to write certificate in PEM format.";
}

util::Status generateRSAKeyPair(EVP_PKEY* evp, int bits) {
  // Generate an RSA keypair.
  BIGNUM_ptr exp(BN_new(), BN_free);
  BN_set_word(exp.get(), RSA_F4);
  RSA* rsa = RSA_new();
  CHECK_RETURN_IF_FALSE(RSA_generate_key_ex(rsa, bits, exp.get(), NULL))
      << "Failed to generate RSA key.";

  // Store this keypair in evp
  // It will be freed when EVP is freed, so only free on failure.
  if (!EVP_PKEY_assign_RSA(evp, rsa)) {
    RSA_free(rsa);
    RETURN_ERROR(ERR_INVALID_PARAM) << "Failed to assign key.";
  }
  return util::OkStatus();
}

util::Status generateSignedCert(X509* unsigned_cert,
                                EVP_PKEY* unsigned_cert_key, X509* issuer,
                                EVP_PKEY* issuer_key,
                                const std::string& common_name, int serial,
                                int days) {
  RETURN_IF_ERROR(generateUnsignedCert(unsigned_cert, unsigned_cert_key,
                                       common_name, serial, days));
  RETURN_IF_ERROR(signCert(unsigned_cert, unsigned_cert_key, issuer, issuer_key,
                           common_name, serial, days));
  return util::OkStatus();
}

util::Status generateUnsignedCert(X509* unsigned_cert,
                                  EVP_PKEY* unsigned_cert_key,
                                  const std::string& common_name, int serial,
                                  int days) {
  CHECK_RETURN_IF_FALSE(
      ASN1_INTEGER_set(X509_get_serialNumber(unsigned_cert), serial));
  CHECK_RETURN_IF_FALSE(X509_gmtime_adj(X509_get_notBefore(unsigned_cert), 0));
  CHECK_RETURN_IF_FALSE(X509_gmtime_adj(X509_get_notAfter(unsigned_cert),
                                        (long)60 * 60 * 24 * days));
  CHECK_RETURN_IF_FALSE(X509_set_pubkey(unsigned_cert, unsigned_cert_key));

  X509_NAME* name = X509_get_subject_name(unsigned_cert);

  CHECK_RETURN_IF_FALSE(X509_NAME_add_entry_by_txt(
      name, "CN", MBSTRING_UTF8,
      reinterpret_cast<const unsigned char*>(common_name.c_str()), -1, -1, 0));
  return util::OkStatus();
}

util::Status signCert(X509* unsigned_cert, EVP_PKEY* unsigned_cert_key,
                      X509* issuer, EVP_PKEY* issuer_key,
                      const std::string& common_name, int serial, int days) {
  X509_NAME* name = X509_get_subject_name(unsigned_cert);
  X509_NAME* issuer_name;
  if (issuer == NULL || issuer_key == NULL) {
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

namespace {

// using BN_ptr = std::unique_ptr<BIGNUM, decltype(&::BN_free)>;
// using RSA_ptr = std::unique_ptr<RSA, decltype(&::RSA_free)>;
// using BIO_MEM_ptr = std::unique_ptr<BIO, decltype(&::BIO_free)>;
// using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, decltype(&::EVP_PKEY_free)>;
#define OPENSSL_PTR_DEFINITION1(type) OPENSSL_PTR_DEFINITION2(type, type)
#define OPENSSL_PTR_DEFINITION2(type, fn) using type##_ptr = std::unique_ptr<type, decltype(&::fn##_free)>
#define OPENSSL_PTR(type, name, ...) type##_ptr name(type##_new(__VA_ARGS__), ::type##_free)

// OPENSSL_PTR_DEFINITION2(BIGNUM, BN);
// OPENSSL_PTR_DEFINITION1(RSA);
// OPENSSL_PTR_DEFINITION1(BIO_MEM);
// OPENSSL_PTR_DEFINITION1(EVP_PKEY);

class RsaCertificate : public Certificate {
public:
  // RsaCertificate() {
  //   // OPENSSL_PTR(BN, bn,);
  //   // OPENSSL_PTR(BIO_MEM, bio, BIO_s_mem());
  // }
  RsaCertificate(const std::string& common_name, int serial_number, int bits = 1024);

  util::StatusOr<std::string> GetPrivateKey() override;
  util::StatusOr<std::string> GetCertificate() override;

private:
  EVP_PKEY_ptr key_;
  X509_ptr certificate_;
  std::string common_name_;
  int serial_number_;

};

RsaCertificate::RsaCertificate(const std::string& common_name, int serial_number, int bits) :
    key_(EVP_PKEY_ptr(EVP_PKEY_new(), EVP_PKEY_free)),
    certificate_(X509_ptr(X509_new(), X509_free)),    
    common_name_(common_name),
    serial_number_(serial_number) {}

util::StatusOr<std::string> RsaCertificate::GetPrivateKey() {
  if (!private_key_string_.empty()) return private_key_string_;
  CHECK_RETURN_IF_FALSE(key_.get()) << "Failed to key.";
  auto result = GetRSAPrivateKeyAsString(key_.get());
  if (result.ok()) private_key_string_ = result.ValueOrDie();
  return result;
}

util::StatusOr<std::string> RsaCertificate::GetCertificate() {
  if (!certificate_string_.empty()) return certificate_string_;
  CHECK_RETURN_IF_FALSE(certificate_.get()) << "Failed to certificate.";
  auto result = GetCertAsString(certificate_.get());
  if (result.ok()) certificate_string_ = result.ValueOrDie();
  return result;
}


}  // namespace

Certificate Certificate::GenerateCertificate(const std::string& common_name, int serial_number, int bits) {
  return RsaCertificate(common_name, serial_number, bits);
}

Certificate::Certificate() {}

Certificate::Certificate(const std::string& certificate, const std::string& private_key) :
    private_key_string_(private_key),
    certificate_string_(certificate) {
  //TODO(bocon): extract certificate??
}

util::StatusOr<std::string> Certificate::GetPrivateKey() {
  return private_key_string_;
}

util::StatusOr<std::string> Certificate::GetCertificate(){
  return certificate_string_;
}

}  // namespace stratum
