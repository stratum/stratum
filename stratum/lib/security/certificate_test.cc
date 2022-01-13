// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/security/certificate.h"

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "openssl/bio.h"
#include "openssl/pem.h"
#include "stratum/glue/status/status_test_util.h"

namespace stratum {

constexpr int KEY_SIZE = 1024;
constexpr char CA_COMMON_NAME[] = "Stratum CA";
constexpr char SERVER_COMMON_NAME[] = "stratum.local";
constexpr int CA_SERIAL_NUMBER = 1;

class CertificateTest : public ::testing::Test {
 protected:
  CertificateTest() : ca_(CA_COMMON_NAME), server_(SERVER_COMMON_NAME) {}

  void SetUp() override {
    absl::Time valid_after = absl::Now();
    absl::Time valid_until = valid_after + absl::Hours(24);

    ASSERT_OK(ca_.GenerateKeyPair(KEY_SIZE));
    ASSERT_OK(ca_.SignCertificate(ca_, valid_after, valid_until,
                                  CA_SERIAL_NUMBER));  // Self-sign.

    ASSERT_OK(server_.GenerateKeyPair(KEY_SIZE));
    ASSERT_OK(server_.SignCertificate(ca_, valid_after, valid_until));
  }

  Certificate ca_;
  Certificate server_;
};

TEST_F(CertificateTest, CheckCommonName) {
  EXPECT_EQ(ca_.GetCommonName(), CA_COMMON_NAME);
  // Don't CheckCommonName on CA cert because the name is not well formed.
  EXPECT_EQ(server_.GetCommonName(), SERVER_COMMON_NAME);
  EXPECT_OK(server_.CheckCommonName(SERVER_COMMON_NAME));
}

TEST_F(CertificateTest, CheckIsCa) {
  EXPECT_OK(ca_.IsCA());
  EXPECT_EQ(ca_.IsCA().ValueOrDie(), true);

  EXPECT_OK(server_.IsCA());
  EXPECT_EQ(server_.IsCA().ValueOrDie(), false);
}

TEST_F(CertificateTest, LoadCaCert) {
  Certificate validate_ca("");
  validate_ca.LoadCertificate(ca_.GetCertificate().ValueOrDie(),
                              ca_.GetPrivateKey().ValueOrDie());
  EXPECT_EQ(validate_ca.GetCommonName(), CA_COMMON_NAME);
  EXPECT_OK(validate_ca.IsCA());
  EXPECT_EQ(validate_ca.IsCA().ValueOrDie(), true);
}

TEST_F(CertificateTest, LoadServerCert) {
  Certificate validate_server("");
  validate_server.LoadCertificate(server_.GetCertificate().ValueOrDie(),
                                  server_.GetPrivateKey().ValueOrDie());
  EXPECT_EQ(validate_server.GetCommonName(), SERVER_COMMON_NAME);
  EXPECT_OK(validate_server.IsCA());
  EXPECT_EQ(validate_server.IsCA().ValueOrDie(), false);
}

TEST_F(CertificateTest, CheckPrivateKey) {
  EXPECT_OK(ca_.GetPrivateKey());
  EXPECT_OK(server_.GetPrivateKey());

  EXPECT_THAT(ca_.GetPrivateKey().ValueOrDie(),
              ::testing::HasSubstr("RSA PRIVATE KEY"));
  EXPECT_THAT(server_.GetPrivateKey().ValueOrDie(),
              ::testing::HasSubstr("RSA PRIVATE KEY"));
}

TEST_F(CertificateTest, CheckCertificate) {
  ASSERT_OK_AND_ASSIGN(std::string ca_cert, ca_.GetCertificate());
  EXPECT_THAT(ca_cert, ::testing::HasSubstr("BEGIN CERTIFICATE"));

  ASSERT_OK_AND_ASSIGN(std::string server_cert, server_.GetCertificate());
  EXPECT_THAT(server_cert, ::testing::HasSubstr("BEGIN CERTIFICATE"));
}

TEST_F(CertificateTest, CheckIssuer) {
  EXPECT_OK(ca_.CheckIssuer(ca_));
  EXPECT_OK(server_.CheckIssuer(ca_));
}

TEST_F(CertificateTest, CheckIssuerFail) {
  auto status = ca_.CheckIssuer(server_);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.ToString(),
              ::testing::HasSubstr("Issuer and cert do not match"));
}

TEST_F(CertificateTest, SerialNumber) {
  // CA serial is explicitly assigned to 1
  ASSERT_OK_AND_ASSIGN(std::string ca_serial, ca_.GetSerialNumber());
  EXPECT_EQ(ca_serial, absl::StrFormat("%02x", CA_SERIAL_NUMBER));

  // Server serial is randomly generated
  //   128 bits yields a 32 character hex string
  ASSERT_OK_AND_ASSIGN(std::string server_serial, server_.GetSerialNumber());
  EXPECT_EQ(server_serial.length(), 16 * 2);
}

}  // namespace stratum
