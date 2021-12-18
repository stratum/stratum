// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/security/certificate.h"

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "openssl/bio.h"
#include "openssl/pem.h"
#include "stratum/glue/status/status_test_util.h"

namespace stratum {

TEST(CertificateTest, Generate_Cert) {
  absl::Time valid_until = absl::Now() + absl::Hours(24);
  Certificate ca("Stratum CA", 1);
  EXPECT_OK(ca.GenerateKeyPair(1024));
  EXPECT_OK(ca.SignCertificate(ca, valid_until));  // Self-sign.
  EXPECT_OK(ca.GetPrivateKey());
  EXPECT_OK(ca.GetCertificate());

  Certificate stratum("stratum.local", 1);
  EXPECT_OK(stratum.GenerateKeyPair(1024));
  EXPECT_OK(stratum.SignCertificate(ca, valid_until));
  EXPECT_OK(stratum.GetPrivateKey());
  EXPECT_OK(stratum.GetCertificate());
}

}  // namespace stratum
