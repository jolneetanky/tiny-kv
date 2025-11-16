#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../src/sstable_manager/sstable_manager_impl.h"           // adjust include path if needed
#include "../src/sstable_file_manager/sstable_file_manager.h"       // interface
#include "../src/types/entry.h"
#include "../src/types/error.h"
#include "../src/types/timestamp.h"

struct GroupL0OverlapsTest : testing::Test {
    SSTableManagerImpl ssTableManagerImpl;
};

// ------ Tests ------

// 1) Test that for Level 0, non-overlapping ranges are in separate groups. Input out of order to verify internal sort.
// And test that overlapping ranges are in the same group.
// TEST_F(GroupL0)

// 2) Test files before and after compaction