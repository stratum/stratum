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

/* TODO(bocon): Copied from a newer version of x509.h
                Remove when present in the version we use.
 */
#ifndef X509_VERSION_3
#define X509_VERSION_3 2
#else
#error "X509_VERSION_3 defined. Remove redefinition block."
#endif

namespace stratum {

// Helper functions around OpenSSL.
namespace {

// RFC 5280 allows serials numbers up to 20 bytes
constexpr int SERIAL_NUMBER_LENGTH_BITS = 16 * 8;  // 16 bytes

bssl::UniquePtr<BIO> NewBio() {
  bssl::UniquePtr<BIO> bio(BIO_new(BIO_s_mem()));
  if (!bio.get()) {
    LOG(FATAL) << "Failed to allocate OpenSSL buffer.";
  }
  return bio;
}

std::string BioToString(const bssl::UniquePtr<BIO>& bio) {
  BUF_MEM* mem = nullptr;
  // Returns a reference to the underlying bio; no need to free.
  BIO_get_mem_ptr(bio.get(), &mem);
  if (mem != nullptr && mem->data && mem->length) {
    return std::string(mem->data, mem->length);
  }
  return "";
}

::util::StatusOr<bssl::UniquePtr<BIO>> StringToBio(const std::string& str) {
  bssl::UniquePtr<BIO> bio = NewBio();
  if (BIO_write(bio.get(), str.data(), str.size()) != str.size()) {
    RETURN_ERROR(ERR_INTERNAL) << "Failed to write string to BIO.";
  }
  return bio;
}

// A macro for running an OpenSSL function and returning a
// on error with a util::Status that contains the OpenSSL error.
#define OPENSSL_RETURN_IF_ERROR(cond)                 \
  ERR_clear_error();                                  \
  if (!ABSL_PREDICT_TRUE(cond)) {                     \
    auto bio = NewBio();                              \
    ERR_print_errors(bio.get());                      \
    RETURN_ERROR(ERR_INVALID_PARAM)                   \
        << "OpenSSL call '" << #cond << "' failed.\n" \
        << BioToString(bio);                          \
  }

util::StatusOr<std::string> GetRSAPrivateKeyAsString(EVP_PKEY* pkey) {
  RSA* rsa;
  // Returns a reference to the underlying key; no need to free.
  OPENSSL_RETURN_IF_ERROR(rsa = EVP_PKEY_get0_RSA(pkey));

  bssl::UniquePtr<BIO> bio = NewBio();
  OPENSSL_RETURN_IF_ERROR(PEM_write_bio_RSAPrivateKey(
      bio.get(), rsa, nullptr, nullptr, 0, nullptr, nullptr));
  return BioToString(bio);
}

util::StatusOr<std::string> GetCertAsString(X509* x509) {
  bssl::UniquePtr<BIO> bio = NewBio();
  OPENSSL_RETURN_IF_ERROR(PEM_write_bio_X509(bio.get(), x509));
  return BioToString(bio);
}

util::Status GenerateRSAKeyPair(EVP_PKEY* evp, int bits) {
  bssl::UniquePtr<BIGNUM> exp(BN_new());
  BN_set_word(exp.get(), RSA_F4);

  bssl::UniquePtr<RSA> rsa(RSA_new());
  OPENSSL_RETURN_IF_ERROR(
      RSA_generate_key_ex(rsa.get(), bits, exp.get(), nullptr));

  // Store this keypair in evp
  OPENSSL_RETURN_IF_ERROR(EVP_PKEY_assign_RSA(evp, rsa.get()));
  // RSA will be freed when EVP is freed, so release the pointer to avoid
  // deletion. Note: RSA will still be deleted by the shared pointer on error.
  rsa.release();

  return ::util::OkStatus();
}

util::Status GenerateUnsignedCert(X509* unsigned_cert,
                                  EVP_PKEY* unsigned_cert_key,
                                  const std::string& common_name,
                                  absl::Time valid_after,
                                  absl::Time valid_until, int serial) {
  // RFC 5280 4.1.2.1.  Version
  OPENSSL_RETURN_IF_ERROR(X509_set_version(unsigned_cert, X509_VERSION_3));

  // RFC 5280 4.1.2.2.  Serial Number
  //   MUST be a positive integer assigned by the CA (up to 20 octets)
  //   (issuer name, serial number) MUST be unique
  ASN1_INTEGER* asn_serial = X509_get_serialNumber(unsigned_cert);
  if (serial > 0) {
    OPENSSL_RETURN_IF_ERROR(ASN1_INTEGER_set(asn_serial, serial));
  } else {  // serial should be randomly generated
    bssl::UniquePtr<BIGNUM> bn(BN_new());
    // Set the MSB to 1 so that all serial numbers are the same length in string
    // form
    OPENSSL_RETURN_IF_ERROR(BN_rand(bn.get(), SERIAL_NUMBER_LENGTH_BITS,
                                    BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY));
    OPENSSL_RETURN_IF_ERROR(BN_to_ASN1_INTEGER(bn.get(), asn_serial));
  }

  // RFC 5280 4.1.2.5.  Validity
  time_t t = absl::ToTimeT(valid_after);
  OPENSSL_RETURN_IF_ERROR(
      X509_time_adj_ex(X509_getm_notBefore(unsigned_cert), 0, 0, &t));
  t = absl::ToTimeT(valid_until);
  OPENSSL_RETURN_IF_ERROR(
      X509_time_adj_ex(X509_getm_notAfter(unsigned_cert), 0, 0, &t));

  // RFC 5280 4.1.2.6.  Subject
  X509_NAME* name = X509_get_subject_name(unsigned_cert);
  OPENSSL_RETURN_IF_ERROR(X509_NAME_add_entry_by_txt(
      name, "CN", MBSTRING_UTF8,
      reinterpret_cast<const unsigned char*>(common_name.c_str()), -1, -1, 0));

  OPENSSL_RETURN_IF_ERROR(X509_set_pubkey(unsigned_cert, unsigned_cert_key));

  return ::util::OkStatus();
}

util::Status AddExtension(X509* cert, X509V3_CTX* ctx, const std::string& name,
                          const std::string& value) {
  bssl::UniquePtr<X509_EXTENSION> ext;
  // Create the extension (UniquePtr will handle free on error)
  OPENSSL_RETURN_IF_ERROR(ext =
                              bssl::UniquePtr<X509_EXTENSION>(X509V3_EXT_nconf(
                                  nullptr, ctx, const_cast<char*>(name.c_str()),
                                  const_cast<char*>(value.c_str()))));

  // Add the extension to the end of the list
  OPENSSL_RETURN_IF_ERROR(X509_add_ext(cert, ext.get(), -1));

  return ::util::OkStatus();
}

util::Status AddX509v3Extensions(X509* cert, X509* issuer, bool self_signed,
                                 bool server_auth) {
  X509V3_CTX ctx;
  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, issuer, cert, NULL, NULL, 0);

  if (!self_signed) {
    // RFC 5280 4.2.1.1.  Authority Key Identifier (required)
    // Exception: MAY be omitted for self-signed certs
    RETURN_IF_ERROR(
        AddExtension(cert, &ctx, "authorityKeyIdentifier", "keyid"));
  }

  // RFC 5280 4.2.1.2.  Subject Key Identifier (required)
  RETURN_IF_ERROR(AddExtension(cert, &ctx, "subjectKeyIdentifier", "hash"));

  // RFC 5280 4.2.1.3.  Key Usage (required)
  std::string key_usage = "critical, digitalSignature";
  if (self_signed) key_usage += ", keyCertSign, cRLSign";
  RETURN_IF_ERROR(AddExtension(cert, &ctx, "keyUsage", key_usage));

  // TODO(bocon): implement RFC 5280 4.2.1.4.  Certificate Policies (required)
  // if (!self_signed) {
  // 2.5.29.32.0 - "anyPolicy"
  //    Wildcard certificate policy matches all other policies unless
  //    inhibited by another extension (defined in RFC 4280 4.2.1.14)
  // Omitted for self-signed certs
  // RETURN_IF_ERROR(
  //     AddExtension(cert, &ctx, "certificatePolicies", "2.5.29.32.0"));
  // }

  // RFC 5280 4.2.1.9.  Basic Constraints (required)
  std::string is_ca = "critical, CA:";
  is_ca += self_signed ? "TRUE" : "FALSE";
  RETURN_IF_ERROR(AddExtension(cert, &ctx, "basicConstraints", is_ca));

  if (server_auth) {
    // RFC 5280 4.2.1.12.  Extended Key Usage (optional)
    RETURN_IF_ERROR(AddExtension(cert, &ctx, "extendedKeyUsage", "serverAuth"));
  }

  return ::util::OkStatus();
}

util::Status SignCert(X509* unsigned_cert, EVP_PKEY* unsigned_cert_key,
                      X509* issuer, EVP_PKEY* issuer_key) {
  bool self_signed = false;
  X509_NAME* name = X509_get_subject_name(unsigned_cert);
  X509_NAME* issuer_name;
  if (!issuer || !issuer_key) {
    // then self sign the cert
    self_signed = true;
    issuer_name = name;
    issuer_key = unsigned_cert_key;
  } else {
    issuer_name = X509_get_subject_name(issuer);
  }
  OPENSSL_RETURN_IF_ERROR(X509_set_issuer_name(unsigned_cert, issuer_name));

  RETURN_IF_ERROR(
      AddX509v3Extensions(unsigned_cert, issuer, self_signed, !self_signed));

  OPENSSL_RETURN_IF_ERROR(X509_sign(unsigned_cert, issuer_key, EVP_sha256()));

  return ::util::OkStatus();
}

util::Status GenerateSignedCert(X509* unsigned_cert,
                                EVP_PKEY* unsigned_cert_key, X509* issuer,
                                EVP_PKEY* issuer_key,
                                const std::string& common_name,
                                absl::Time valid_after, absl::Time valid_until,
                                int serial) {
  RETURN_IF_ERROR(GenerateUnsignedCert(unsigned_cert, unsigned_cert_key,
                                       common_name, valid_after, valid_until,
                                       serial));
  RETURN_IF_ERROR(
      SignCert(unsigned_cert, unsigned_cert_key, issuer, issuer_key));

  return ::util::OkStatus();
}

util::Status CheckSignatures(X509* cert, X509* ca) {
  bssl::UniquePtr<X509_STORE> store;
  bssl::UniquePtr<X509_STORE_CTX> store_ctx;
  OPENSSL_RETURN_IF_ERROR(store =
                              bssl::UniquePtr<X509_STORE>(X509_STORE_new()));
  OPENSSL_RETURN_IF_ERROR(
      store_ctx = bssl::UniquePtr<X509_STORE_CTX>(X509_STORE_CTX_new()));

  X509_STORE_CTX_init(store_ctx.get(), store.get(), cert, nullptr);

  // Note: Can't use bssl::UniquePtr because that uses sk_X509_pop_free which
  // double frees
  auto sk = std::unique_ptr<STACK_OF(X509), decltype(&::sk_X509_free)>(
      sk_X509_new_null(), sk_X509_free);
  OPENSSL_RETURN_IF_ERROR(sk_X509_push(sk.get(), ca));
  X509_STORE_CTX_trusted_stack(store_ctx.get(), sk.get());

  if (X509_verify_cert(store_ctx.get()) <= 0) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Error verifying certificate chain: "
        << X509_verify_cert_error_string(store_ctx.get()->error);
  }

  return ::util::OkStatus();
}

}  // namespace

Certificate::Certificate(const std::string& common_name)
    : key_(bssl::UniquePtr<EVP_PKEY>(EVP_PKEY_new())),
      certificate_(bssl::UniquePtr<X509>(X509_new())),
      common_name_(common_name) {}

util::StatusOr<std::string> Certificate::GetPrivateKey() {
  return GetRSAPrivateKeyAsString(key_.get());
}

util::StatusOr<std::string> Certificate::GetCertificate() {
  return GetCertAsString(certificate_.get());
}

util::Status Certificate::GenerateKeyPair(int bits) {
  return GenerateRSAKeyPair(key_.get(), bits);
}

util::StatusOr<bool> Certificate::IsCA() {
  switch (X509_check_ca(certificate_.get())) {
    case 0:  // not CA certificate
      return false;
    case 1:  // X509v3 CA certificate with basicConstraints
    case 3:  // self-signed X509 v1 certificate
      return true;
    case 4:  // X509v3 certificate with keyCertSign and without basicConstraints
    case 5:  // outdated Netscape Certificate Type
      RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid certificate.";
    default:
      RETURN_ERROR(ERR_INTERNAL)
          << "OpenSSL error while checking if cert is CA.";
  }
}

util::Status Certificate::CheckCommonName(const std::string& name) {
  switch (X509_check_host(certificate_.get(), name.data(), name.size(), 0,
                          nullptr)) {
    case 1:  // success
      return ::util::OkStatus();
    case 0:  // failed to match
      RETURN_ERROR(ERR_ENTRY_NOT_FOUND) << "Common name does not match.";
    case -2:  // malformed input
      RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid name param.";
    case -1:  // internal error
    default:
      RETURN_ERROR(ERR_INTERNAL) << "OpenSSL error while checking common name.";
  }
}

const std::string& Certificate::GetCommonName() { return common_name_; }

util::StatusOr<std::string> Certificate::GetSerialNumber() {
  if (!certificate_)
    RETURN_ERROR(ERR_INTERNAL) << "Certificate is not yet generated or loaded.";
  bssl::UniquePtr<BIGNUM> num(
      ASN1_INTEGER_to_BN(X509_get0_serialNumber(certificate_.get()), nullptr));
  if (!num)
    RETURN_ERROR(ERR_INTERNAL)
        << "Failed to get serial number from certificate.";
  bssl::UniquePtr<char> str(BN_bn2hex(num.get()));
  if (!str) RETURN_ERROR(ERR_INTERNAL) << "Failed to generate hex string.";
  return std::string(str.get());
}

util::Status Certificate::CheckIssuer(const Certificate& issuer) {
  // Verify fields in the certificate are consistent (but not signatures)
  int ret;
  if ((ret = X509_check_issued(issuer.certificate_.get(),
                               certificate_.get())) != X509_V_OK) {
    RETURN_ERROR(ERR_INTERNAL) << "Issuer and cert do not match: "
                               << X509_verify_cert_error_string(ret);
  }

  // Check that signature of this certificate was signed by the issuer
  RETURN_IF_ERROR(
      CheckSignatures(certificate_.get(), issuer.certificate_.get()));

  return ::util::OkStatus();
}

util::Status Certificate::LoadCertificate(const std::string& cert_pem,
                                          const std::string& key_pem) {
  // Load private key from PEM string
  ASSIGN_OR_RETURN(bssl::UniquePtr<BIO> key_bio, StringToBio(key_pem));
  bssl::UniquePtr<RSA> rsa;
  OPENSSL_RETURN_IF_ERROR(rsa = bssl::UniquePtr<RSA>(PEM_read_bio_RSAPrivateKey(
                              key_bio.get(), nullptr, nullptr, nullptr)));
  // Store this keypair in EVP
  OPENSSL_RETURN_IF_ERROR(EVP_PKEY_assign_RSA(key_.get(), rsa.get()));
  // RSA will be freed when EVP is freed, so release the pointer to avoid
  // deletion. Note: RSA will still be deleted by the shared pointer on error.
  rsa.release();

  // Load certificate from PEM string
  ASSIGN_OR_RETURN(bssl::UniquePtr<BIO> cert_bio, StringToBio(cert_pem));
  OPENSSL_RETURN_IF_ERROR(certificate_ =
                              bssl::UniquePtr<X509>(PEM_read_bio_X509(
                                  cert_bio.get(), nullptr, nullptr, nullptr)));

  // Copy common name from loaded certificate
  X509_NAME* name;
  OPENSSL_RETURN_IF_ERROR(name = X509_get_subject_name(certificate_.get()));
  bssl::UniquePtr<BIO> name_bio = NewBio();
  OPENSSL_RETURN_IF_ERROR(
      X509_NAME_print_ex(name_bio.get(), name, 0,
                         ASN1_STRFLGS_RFC2253 | ASN1_STRFLGS_ESC_QUOTE |
                             XN_FLAG_SEP_CPLUS_SPC | XN_FLAG_FN_NONE));
  common_name_ = BioToString(name_bio);

  // Check that the certificate and private key match
  ERR_clear_error();
  switch (X509_check_private_key(certificate_.get(), key_.get())) {
    case 1:  // cert and key match
      break;
    case 0:  // cert and key do not match
      RETURN_ERROR(ERR_INVALID_PARAM)
          << "Certificate and private key do not match.";
    default:
      auto bio = NewBio();
      ERR_print_errors(bio.get());
      RETURN_ERROR(ERR_INVALID_PARAM) << "OpenSSL error checking private key.\n"
                                      << BioToString(bio);
  }

  return ::util::OkStatus();
}

util::Status Certificate::SignCertificate(const Certificate& issuer,
                                          absl::Time valid_after,
                                          absl::Time valid_until, int serial) {
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
                            issuer_key, common_name_, valid_after, valid_until,
                            serial);
}

}  // namespace stratum
