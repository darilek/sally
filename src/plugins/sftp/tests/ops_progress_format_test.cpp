// Tests for ops/progress_format: number formatting and the status decision of the operation window.
#include <gtest/gtest.h>

#include "progress_format.h"

TEST(ProgressFormat, Bytes)
{
    EXPECT_EQ(FormatBytes(0), L"0 B");
    EXPECT_EQ(FormatBytes(1023), L"1023 B");
    EXPECT_EQ(FormatBytes(1024), L"1.0 KB");
    EXPECT_EQ(FormatBytes(1536), L"1.5 KB");
    EXPECT_EQ(FormatBytes(5ull * 1024 * 1024), L"5.0 MB");
    EXPECT_EQ(FormatBytes(3ull * 1024 * 1024 * 1024), L"3.0 GB");
    EXPECT_EQ(FormatBytes(1024ull * 1024 - 1), L"1.0 MB"); // not "1024.0 KB"
    EXPECT_FALSE(FormatBytes(UINT64_MAX).empty()); // the largest unit is PB: no index out of range
}

TEST(ProgressFormat, Speed)
{
    EXPECT_EQ(FormatSpeed(0), L"0 B/s");
    EXPECT_EQ(FormatSpeed(-5), L"0 B/s");
    EXPECT_EQ(FormatSpeed(2048), L"2.0 KB/s");
}

TEST(ProgressFormat, DurationAndEta)
{
    EXPECT_EQ(FormatDuration(0), L"0:00");
    EXPECT_EQ(FormatDuration(65), L"1:05");
    EXPECT_EQ(FormatDuration(3599), L"59:59");
    EXPECT_EQ(FormatDuration(3600), L"1:00:00");
    EXPECT_EQ(FormatDuration(90061), L"25:01:01");
    EXPECT_EQ(FormatDuration(-3), L"0:00");
    EXPECT_EQ(FormatEta(-1), L"--:--");
    EXPECT_EQ(FormatEta(0), L"0:00");
    EXPECT_EQ(FormatEta(75), L"1:15");
}

TEST(ProgressFormat, Percent)
{
    EXPECT_EQ(PercentDone(0, 0), 0);
    EXPECT_EQ(PercentDone(5, 0), 0);
    EXPECT_EQ(PercentDone(0, 100), 0);
    EXPECT_EQ(PercentDone(50, 100), 50);
    EXPECT_EQ(PercentDone(99, 100), 99);
    EXPECT_EQ(PercentDone(100, 100), 100);
    EXPECT_EQ(PercentDone(150, 100), 100);
    const int half = PercentDone(UINT64_MAX / 2, UINT64_MAX); // no overflow
    EXPECT_GE(half, 49);
    EXPECT_LE(half, 50);
}

TEST(ProgressFormat, StatusDecision)
{
    EXPECT_EQ(DecideStatus(OperationState::Running, false, false, false), OperationStatus::Running);
    EXPECT_EQ(DecideStatus(OperationState::Running, true, false, false), OperationStatus::Paused);
    EXPECT_EQ(DecideStatus(OperationState::Finished, false, false, true), OperationStatus::Finished);
    EXPECT_EQ(DecideStatus(OperationState::FinishedWithSkips, false, false, true), OperationStatus::FinishedWithSkips);
    EXPECT_EQ(DecideStatus(OperationState::FinishedWithErrors, false, false, true), OperationStatus::FinishedWithErrors);
    EXPECT_EQ(DecideStatus(OperationState::NeedsUser, false, false, true), OperationStatus::NeedsUser);
    // a cancel wins over everything else
    EXPECT_EQ(DecideStatus(OperationState::Running, true, true, false), OperationStatus::Cancelling);
    EXPECT_EQ(DecideStatus(OperationState::Running, false, true, true), OperationStatus::Cancelled);
    EXPECT_EQ(DecideStatus(OperationState::Finished, false, true, true), OperationStatus::Cancelled);
}

TEST(ProgressFormat, WorkOver)
{
    EXPECT_FALSE(IsWorkOver(OperationState::Running));
    EXPECT_TRUE(IsWorkOver(OperationState::Finished));
    EXPECT_TRUE(IsWorkOver(OperationState::NeedsUser));
}
