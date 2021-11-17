

#include "stratum/lib/security/cert_utils.h"

#include "openssl/bio.h"
#include "openssl/bn.h"
#include "openssl/buffer.h"
#include "openssl/evp.h"
#include "openssl/pem.h"
#include "openssl/rsa.h"
#include "stratum/lib/macros.h"

namespace stratum {

util::StatusOr<std::string> getRSAPrivateKeyAsString(EVP_PKEY* pkey) {
  RSA* rsa = EVP_PKEY_get0_RSA(pkey);
  CHECK_RETURN_IF_FALSE(rsa) << "not an rsa key";

  BIO* privateKeyBio = BIO_new(BIO_s_mem());

  CHECK_RETURN_IF_FALSE(PEM_write_bio_RSAPrivateKey(privateKeyBio, rsa, NULL,
                                                    NULL, 0, NULL, NULL));

  BUF_MEM* mem = BUF_MEM_new();
  CHECK_RETURN_IF_FALSE(BIO_get_mem_ptr(privateKeyBio, &mem));
  if (mem->data && mem->length) {
    std::string private_key(mem->data, mem->length);
    BIO_free(privateKeyBio);
    return private_key;
  }

  BIO_free(privateKeyBio);
  RETURN_ERROR(ERR_INVALID_PARAM) << "could not read private key";
}

util::StatusOr<std::string> getCertAsString(X509* x509) {
  BIO* bio = BIO_new(BIO_s_mem());

  CHECK_RETURN_IF_FALSE(PEM_write_bio_X509(bio, x509));

  BUF_MEM* mem = BUF_MEM_new();
  CHECK_RETURN_IF_FALSE(BIO_get_mem_ptr(bio, &mem));
  if (mem->data && mem->length) {
    std::string s(mem->data, mem->length);
    BIO_free(bio);
    return s;
  }

  BIO_free(bio);
  RETURN_ERROR(ERR_INVALID_PARAM) << "could not print cert object";
}

util::Status generateRSAKeyPair(EVP_PKEY* evp, int bits) {
  // Generate an RSA keypair.
  BIGNUM* exp = BN_new();
  BN_set_word(exp, RSA_F4);
  RSA* rsa = RSA_new();
  CHECK_RETURN_IF_FALSE(RSA_generate_key_ex(rsa, bits, exp, NULL))
      << "Failed to generate RSA key";

  // Store this keypair in evp.
  CHECK_RETURN_IF_FALSE(EVP_PKEY_assign_RSA(evp, rsa));
  BN_free(exp);
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
      name, "CN", MBSTRING_ASC,
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

}  // namespace stratum
