// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/system_time_provider.h"

#include "base/macros.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement_tracker {

namespace {

base::Time GetTime(int year, int month, int day) {
  base::Time::Exploded exploded_time;
  exploded_time.year = year;
  exploded_time.month = month;
  exploded_time.day_of_month = day;
  exploded_time.hour = 0;
  exploded_time.minute = 0;
  exploded_time.second = 0;
  exploded_time.millisecond = 0;

  base::Time out_time;
  EXPECT_TRUE(base::Time::FromLocalExploded(exploded_time, &out_time));
  return out_time;
}

// A SystemTimeProvider where the current time can be defined at runtime.
class TestSystemTimeProvider : public SystemTimeProvider {
 public:
  TestSystemTimeProvider() = default;

  // SystemTimeProvider implementation.
  base::Time Now() const override { return current_time_; }

  void SetCurrentTime(base::Time time) { current_time_ = time; }

 private:
  base::Time current_time_;

  DISALLOW_COPY_AND_ASSIGN(TestSystemTimeProvider);
};

class SystemTimeProviderTest : public ::testing::Test {
 public:
  SystemTimeProviderTest() = default;

 protected:
  TestSystemTimeProvider time_provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemTimeProviderTest);
};

}  // namespace

TEST_F(SystemTimeProviderTest, EpochIs0Days) {
  time_provider_.SetCurrentTime(base::Time::UnixEpoch());
  EXPECT_EQ(0u, time_provider_.GetCurrentDay());
}

TEST_F(SystemTimeProviderTest, TestDeltasFromEpoch) {
  base::Time epoch = base::Time::UnixEpoch();

  time_provider_.SetCurrentTime(epoch + base::TimeDelta::FromDays(1));
  EXPECT_EQ(1u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(epoch + base::TimeDelta::FromDays(2));
  EXPECT_EQ(2u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(epoch + base::TimeDelta::FromDays(100));
  EXPECT_EQ(100u, time_provider_.GetCurrentDay());
}

TEST_F(SystemTimeProviderTest, TestNegativeDeltasFromEpoch) {
  base::Time epoch = base::Time::UnixEpoch();

  time_provider_.SetCurrentTime(epoch - base::TimeDelta::FromDays(1));
  EXPECT_EQ(0u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(epoch - base::TimeDelta::FromDays(2));
  EXPECT_EQ(0u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(epoch - base::TimeDelta::FromDays(100));
  EXPECT_EQ(0u, time_provider_.GetCurrentDay());
}

TEST_F(SystemTimeProviderTest, TestManualDatesAroundEpoch) {
  time_provider_.SetCurrentTime(GetTime(1970, 1, 1));
  EXPECT_EQ(0u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(GetTime(1970, 1, 2));
  EXPECT_EQ(1u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(GetTime(1970, 4, 11));
  EXPECT_EQ(100u, time_provider_.GetCurrentDay());
}

TEST_F(SystemTimeProviderTest, TestManualDatesAroundGoogleIO2017) {
  time_provider_.SetCurrentTime(GetTime(2017, 5, 17));
  EXPECT_EQ(17303u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(GetTime(2017, 5, 18));
  EXPECT_EQ(17304u, time_provider_.GetCurrentDay());

  time_provider_.SetCurrentTime(GetTime(2017, 5, 19));
  EXPECT_EQ(17305u, time_provider_.GetCurrentDay());
}

}  // namespace feature_engagement_tracker
