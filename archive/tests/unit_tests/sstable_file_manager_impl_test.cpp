#include <gtest/gtest.h>
#include <filesystem>
#include "../../src/sstable_file_manager/sstable_file_manager_impl.h"
#include "../../src/types/entry.h"
#include "../../src/types/error.h"
#include "../../src/contexts/system_context.h"
#include "../factories/mock_bloom_filter/mock_bloom_filter_factory.h"

namespace fs = std::filesystem;

// utility: create and clean up a unique temp dir for each test
struct TempDir
{
    fs::path path;
    TempDir()
    {
        auto base = fs::temp_directory_path();
        path = base / fs::path("tinykv_test_" + std::to_string(::getpid()) + "_" + std::to_string(std::rand()));
        fs::create_directories(path);
    }
    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

struct SSTableFileManagerTest : ::testing::Test
{
    TempDir tmp;
};

// TODO: create a mock SystemContext
TEST_F(SSTableFileManagerTest, ConstructorCreatesFileInDirectory)
{
    MockBloomFilterFactory bff = MockBloomFilterFactory();
    SystemContext sysCtx = SystemContext(bff);

    // before: dir is empty
    ASSERT_TRUE(fs::is_empty(tmp.path));
    // act: call constructor on dir
    SSTableFileManagerImpl fm(tmp.path.string(), sysCtx);
    // assert: directory is no longer empty

    // after: exactly 1 file in that directory
    int count = 0;
    for (auto &p : fs::directory_iterator(tmp.path))
    {
        count++;
    }
    EXPECT_EQ(count, 1);

    // check that fm.getFullPath() points inside tmp.path
    auto fullPath = fm.getFullPath();
    EXPECT_TRUE(fullPath.find(tmp.path.string()) != std::string::npos);
}