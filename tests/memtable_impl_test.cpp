#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../src/skip_list/skip_list.h"
#include "../src/sstable_manager/sstable_manager.h"
#include "../src/wal/wal.h"

// mocks
// class MockSkipList : public SkipList {
//     public:
//         MOCK_METHOD()
// }
//         SkipList &m_skiplist;
//         SSTableManager &m_ssTableManager;
//         WAL &m_wal;