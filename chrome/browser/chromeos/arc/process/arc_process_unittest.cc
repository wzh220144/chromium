// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/process/arc_process.h"

#include <assert.h>
#include <list>

#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

// Tests that ArcProcess objects can be sorted by their priority (higher to
// lower). This is critical for the OOM handler to work correctly.
TEST(ArcProcess, TestSorting) {
  constexpr int64_t kNow = 1234567890;

  std::list<ArcProcess> processes;  // use list<> for emplace_front.
  processes.emplace_back(0, 0, "process 0", mojom::ProcessState::PERSISTENT,
                         false /* is_focused */, kNow + 1);
  processes.emplace_front(1, 1, "process 1", mojom::ProcessState::PERSISTENT,
                          false, kNow);
  processes.emplace_back(2, 2, "process 2", mojom::ProcessState::LAST_ACTIVITY,
                         false, kNow);
  processes.emplace_front(3, 3, "process 3", mojom::ProcessState::LAST_ACTIVITY,
                          false, kNow + 1);
  processes.emplace_back(4, 4, "process 4", mojom::ProcessState::CACHED_EMPTY,
                         false, kNow + 1);
  processes.emplace_front(5, 5, "process 5", mojom::ProcessState::CACHED_EMPTY,
                          false, kNow);
  processes.sort();

  static_assert(
      mojom::ProcessState::PERSISTENT < mojom::ProcessState::LAST_ACTIVITY,
      "unexpected enum values");
  static_assert(
      mojom::ProcessState::LAST_ACTIVITY < mojom::ProcessState::CACHED_EMPTY,
      "unexpected enum values");

  std::list<ArcProcess>::const_iterator it = processes.begin();
  // 0 should have higher priority since its last_activity_time is more recent.
  EXPECT_EQ(0, it->pid());
  ++it;
  EXPECT_EQ(1, it->pid());
  ++it;
  // Same, 3 should have higher priority.
  EXPECT_EQ(3, it->pid());
  ++it;
  EXPECT_EQ(2, it->pid());
  ++it;
  // Same, 4 should have higher priority.
  EXPECT_EQ(4, it->pid());
  ++it;
  EXPECT_EQ(5, it->pid());
}

TEST(ArcProcess, TestIsImportant) {
  constexpr bool kIsNotFocused = false;

  // Processes up to IMPORTANT_FOREGROUND are considered important.
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::PERSISTENT,
                         kIsNotFocused, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::PERSISTENT_UI,
                         kIsNotFocused, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::TOP, true, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::TOP, false, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessState::BOUND_FOREGROUND_SERVICE,
                         kIsNotFocused, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessState::FOREGROUND_SERVICE, kIsNotFocused,
                         0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::TOP_SLEEPING,
                         kIsNotFocused, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessState::IMPORTANT_FOREGROUND,
                         kIsNotFocused, 0)
                  .IsImportant());

  // Others are not important.
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessState::IMPORTANT_BACKGROUND,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(
      ArcProcess(0, 0, "process", mojom::ProcessState::BACKUP, kIsNotFocused, 0)
          .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::HEAVY_WEIGHT,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::SERVICE,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::RECEIVER,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(
      ArcProcess(0, 0, "process", mojom::ProcessState::HOME, kIsNotFocused, 0)
          .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::LAST_ACTIVITY,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::CACHED_ACTIVITY,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessState::CACHED_ACTIVITY_CLIENT,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::CACHED_EMPTY,
                          kIsNotFocused, 0)
                   .IsImportant());
}

TEST(ArcProcess, TestIsKernelKillable) {
  constexpr bool kIsNotFocused = false;

  // Only PERSISITENT* processes are protected from the kernel OOM killer.
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::PERSISTENT,
                          kIsNotFocused, 0)
                   .IsKernelKillable());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessState::PERSISTENT_UI,
                          kIsNotFocused, 0)
                   .IsKernelKillable());

  // Both TOP+focused and TOP apps are still kernel-killable.
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::TOP, true, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::TOP, false, 0)
                  .IsKernelKillable());

  // Others are kernel-killable.
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessState::BOUND_FOREGROUND_SERVICE,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessState::FOREGROUND_SERVICE, kIsNotFocused,
                         0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::TOP_SLEEPING,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessState::IMPORTANT_FOREGROUND,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessState::IMPORTANT_BACKGROUND,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(
      ArcProcess(0, 0, "process", mojom::ProcessState::BACKUP, kIsNotFocused, 0)
          .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::HEAVY_WEIGHT,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::SERVICE,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::RECEIVER,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(
      ArcProcess(0, 0, "process", mojom::ProcessState::HOME, kIsNotFocused, 0)
          .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::LAST_ACTIVITY,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::CACHED_ACTIVITY,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessState::CACHED_ACTIVITY_CLIENT,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessState::CACHED_EMPTY,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
}

}  // namespace

}  // namespace arc
