#include <gtest/gtest.h>

#include "core/level_manager/compaction_planner.h"

namespace
{
    SSTableMetadata table(FileNumber fileNumber, const char *minKey, const char *maxKey)
    {
        return SSTableMetadata{fileNumber, 0, minKey, maxKey};
    }
}

TEST(CompactionPlannerTest, GroupsOverlappingRangesAndOmitsUnrelatedTables)
{
    const std::vector<SSTableMetadata> sourceTables{
        table(10, "a", "d"),
        table(11, "c", "f"),
        table(12, "x", "z"),
    };
    const std::vector<SSTableMetadata> destinationTables{
        table(20, "b", "c"),
        table(21, "f", "h"),
        table(22, "j", "k"),
        table(23, "y", "y"),
    };

    const auto plans = tinykv::CompactionPlanner::planOverlappingGroups(sourceTables, destinationTables);

    ASSERT_EQ(plans.size(), 2);
    EXPECT_EQ(plans[0].sourceFileNumbers, (std::vector<FileNumber>{10, 11}));
    EXPECT_EQ(plans[0].destinationFileNumbers, (std::vector<FileNumber>{20, 21}));
    EXPECT_EQ(plans[0].minKey, "a");
    EXPECT_EQ(plans[0].maxKey, "h");

    EXPECT_EQ(plans[1].sourceFileNumbers, (std::vector<FileNumber>{12}));
    EXPECT_EQ(plans[1].destinationFileNumbers, (std::vector<FileNumber>{23}));
    EXPECT_EQ(plans[1].minKey, "x");
    EXPECT_EQ(plans[1].maxKey, "z");
}

TEST(CompactionPlannerTest, DestinationRangeCanJoinOtherwiseSeparateSourceRanges)
{
    const std::vector<SSTableMetadata> sourceTables{
        table(10, "a", "b"),
        table(11, "d", "e"),
    };
    const std::vector<SSTableMetadata> destinationTables{
        table(20, "b", "d"),
    };

    const auto plans = tinykv::CompactionPlanner::planOverlappingGroups(sourceTables, destinationTables);

    ASSERT_EQ(plans.size(), 1);
    EXPECT_EQ(plans[0].sourceFileNumbers, (std::vector<FileNumber>{10, 11}));
    EXPECT_EQ(plans[0].destinationFileNumbers, (std::vector<FileNumber>{20}));
    EXPECT_EQ(plans[0].minKey, "a");
    EXPECT_EQ(plans[0].maxKey, "e");
}

TEST(CompactionPlannerTest, ReturnsNoPlansWithoutSourceTables)
{
    const std::vector<SSTableMetadata> destinationTables{table(20, "a", "z")};

    const auto plans = tinykv::CompactionPlanner::planOverlappingGroups({}, destinationTables);

    EXPECT_TRUE(plans.empty());
}
