// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/rappor_service.h"

#include "base/base64.h"
#include "base/prefs/testing_pref_service.h"
#include "components/rappor/byte_vector_utils.h"
#include "components/rappor/proto/rappor_metric.pb.h"
#include "components/rappor/rappor_parameters.h"
#include "components/rappor/rappor_pref_names.h"
#include "components/rappor/test_log_uploader.h"
#include "components/rappor/test_rappor_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace rappor {

TEST(RapporServiceTest, LoadCohort) {
  TestRapporService rappor_service;
  rappor_service.test_prefs()->SetInteger(prefs::kRapporCohortSeed, 1);
  EXPECT_EQ(1, rappor_service.LoadCohortForTesting());
}

TEST(RapporServiceTest, LoadSecret) {
  TestRapporService rappor_service;
  std::string secret = HmacByteVectorGenerator::GenerateEntropyInput();
  std::string secret_base64;
  base::Base64Encode(secret, &secret_base64);
  rappor_service.test_prefs()->SetString(prefs::kRapporSecret, secret_base64);
  EXPECT_EQ(secret, rappor_service.LoadSecretForTesting());
}

TEST(RapporServiceTest, Update) {
  TestRapporService rappor_service;
  EXPECT_LT(base::TimeDelta(), rappor_service.next_rotation());
  EXPECT_TRUE(rappor_service.test_uploader()->is_running());

  rappor_service.Update(RECORDING_DISABLED, false);
  EXPECT_EQ(base::TimeDelta(), rappor_service.next_rotation());
  EXPECT_FALSE(rappor_service.test_uploader()->is_running());

  rappor_service.Update(FINE_LEVEL, false);
  EXPECT_LT(base::TimeDelta(), rappor_service.next_rotation());
  EXPECT_FALSE(rappor_service.test_uploader()->is_running());

  rappor_service.Update(COARSE_LEVEL, true);
  EXPECT_LT(base::TimeDelta(), rappor_service.next_rotation());
  EXPECT_TRUE(rappor_service.test_uploader()->is_running());
}

// Check that samples can be recorded and exported.
TEST(RapporServiceTest, RecordAndExportMetrics) {
  TestRapporService rappor_service;

  // Multiple samples for the same metric should only generate one report.
  rappor_service.RecordSample("MyMetric", ETLD_PLUS_ONE_RAPPOR_TYPE, "foo");
  rappor_service.RecordSample("MyMetric", ETLD_PLUS_ONE_RAPPOR_TYPE, "bar");

  RapporReports reports;
  rappor_service.GetReports(&reports);
  EXPECT_EQ(1, reports.report_size());

  const RapporReports::Report& report = reports.report(0);
  EXPECT_TRUE(report.name_hash());
  // ETLD_PLUS_ONE_RAPPOR_TYPE has 128 bits
  EXPECT_EQ(16u, report.bits().size());
}

// Check that the reporting level is respected.
TEST(RapporServiceTest, RecordingLevel) {
  TestRapporService rappor_service;
  rappor_service.Update(COARSE_LEVEL, false);

  // ETLD_PLUS_ONE_RAPPOR_TYPE is a FINE_LEVEL metric
  rappor_service.RecordSample("FineMetric", ETLD_PLUS_ONE_RAPPOR_TYPE, "foo");

  RapporReports reports;
  rappor_service.GetReports(&reports);
  EXPECT_EQ(0, reports.report_size());
}

// Check that the incognito is respected.
TEST(RapporServiceTest, Incognito) {
  TestRapporService rappor_service;
  rappor_service.set_is_incognito(true);

  rappor_service.RecordSample("MyMetric", COARSE_RAPPOR_TYPE, "foo");

  RapporReports reports;
  rappor_service.GetReports(&reports);
  EXPECT_EQ(0, reports.report_size());
}

}  // namespace rappor
