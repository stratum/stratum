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
#include "stratum/public/lib/error.h"

namespace stratum {
namespace {

using test_utils::IsOkAndHolds;
using test_utils::StatusIs;
using testing::HasSubstr;

constexpr int kKeySize = 1024;
constexpr char kCaCommonName[] = "Stratum CA";
constexpr char kServerCommonName[] = "stratum.local";
constexpr int kCaSerialNumber = 1;

class CertificateTest : public ::testing::Test {
 protected:
  CertificateTest() : ca_(kCaCommonName), server_(kServerCommonName) {}

  void SetUp() override {
    absl::Time valid_after = absl::Now();
    absl::Time valid_until = valid_after + absl::Hours(24);

    ASSERT_OK(ca_.GenerateKeyPair(kKeySize));
    ASSERT_OK(ca_.SignCertificate(ca_, valid_after, valid_until,
                                  kCaSerialNumber));  // Self-sign.

    ASSERT_OK(server_.GenerateKeyPair(kKeySize));
    ASSERT_OK(server_.SignCertificate(ca_, valid_after, valid_until));
  }

  Certificate ca_;
  Certificate server_;
};

TEST_F(CertificateTest, CheckCommonName) {
  EXPECT_EQ(ca_.GetCommonName(), kCaCommonName);
  // Don't CheckCommonName on CA cert because the name is not well formed.
  EXPECT_EQ(server_.GetCommonName(), kServerCommonName);
  EXPECT_OK(server_.CheckCommonName(kServerCommonName));
}

TEST_F(CertificateTest, CheckIsCa) {
  EXPECT_THAT(ca_.IsCA(), IsOkAndHolds(true));
  EXPECT_THAT(server_.IsCA(), IsOkAndHolds(false));
}

TEST_F(CertificateTest, LoadCaCert) {
  Certificate validate_ca("");
  validate_ca.LoadCertificate(ca_.GetCertificate().ValueOrDie(),
                              ca_.GetPrivateKey().ValueOrDie());
  EXPECT_EQ(validate_ca.GetCommonName(), kCaCommonName);
  EXPECT_THAT(validate_ca.IsCA(), IsOkAndHolds(true));
}

TEST_F(CertificateTest, LoadServerCert) {
  Certificate validate_server("");
  validate_server.LoadCertificate(server_.GetCertificate().ValueOrDie(),
                                  server_.GetPrivateKey().ValueOrDie());
  EXPECT_EQ(validate_server.GetCommonName(), kServerCommonName);
  EXPECT_THAT(validate_server.IsCA(), IsOkAndHolds(false));
}

TEST_F(CertificateTest, CheckPrivateKey) {
  EXPECT_THAT(ca_.GetPrivateKey(), IsOkAndHolds(HasSubstr("RSA PRIVATE KEY")));
  EXPECT_THAT(server_.GetPrivateKey(),
              IsOkAndHolds(HasSubstr("RSA PRIVATE KEY")));
}

TEST_F(CertificateTest, CheckCertificate) {
  EXPECT_THAT(ca_.GetCertificate(),
              IsOkAndHolds(HasSubstr("BEGIN CERTIFICATE")));
  EXPECT_THAT(server_.GetCertificate(),
              IsOkAndHolds(HasSubstr("BEGIN CERTIFICATE")));
}

TEST_F(CertificateTest, CheckIssuer) {
  EXPECT_OK(ca_.CheckIssuer(ca_));
  EXPECT_OK(server_.CheckIssuer(ca_));
}

TEST_F(CertificateTest, CheckIssuerFail) {
  EXPECT_THAT(ca_.CheckIssuer(server_),
              StatusIs(StratumErrorSpace(), ERR_INTERNAL,
                       HasSubstr("Issuer and cert do not match")));
}

TEST_F(CertificateTest, SerialNumber) {
  // CA serial is explicitly assigned to 1
  EXPECT_THAT(ca_.GetSerialNumber(),
              IsOkAndHolds(absl::StrFormat("%02x", kCaSerialNumber)));

  // Server serial is randomly generated
  //   16 bytes (128 bits) yields a 32 character hex string
  EXPECT_THAT(server_.GetSerialNumber(),
              IsOkAndHolds(::testing::ContainsRegex("^[0-9a-f]{32}$")));
}

}  // namespace
}  // namespace stratum
