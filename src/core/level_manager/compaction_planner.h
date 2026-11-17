#ifndef COMPACTION_PLANNER_H
#define COMPACTION_PLANNER_H

#include "core/sstable_manager/sstable_metadata.h"

#include <cstddef>
#include <string>
#include <vector>

namespace tinykv
{
    // A compaction moves data from an upper level into the next lower level.
    // The source is the upper level whose tables are being compacted; the
    // destination is the lower level that receives the merged output.
    //
    // This plan describes one connected overlap group. File numbers identify
    // the selected tables independently of vector order. minKey and maxKey are
    // the inclusive bounds of the group's combined key range.
    struct CompactionPlan
    {
        // Tables selected from the upper/source level.
        std::vector<FileNumber> sourceFileNumbers;

        // Overlapping tables selected from the lower/destination level.
        std::vector<FileNumber> destinationFileNumbers;

        std::string minKey;
        std::string maxKey;
    };

    class CompactionPlanner
    {
    public:
        // sourceTables are from the upper level being compacted; destinationTables
        // are from the next lower level. Plans groups across all supplied tables.
        // Destination-only groups are omitted. Ranges are inclusive.

        // The input vectors do NOT need to be sorted.
        // The planner combines them and sorts table ranges by `m_min_key` itself.
        // This isolates compaction strategy from LevelManager.

        // ASSUMPTIONS:
        // - Every table has a valid, non-empty inclusive range:
        //   m_min_key <= m_max_key.
        // - Key strings use the same lexicographic ordering used by SSTables.
        // - Each table appears once in its corresponding input vector.
        // - File numbers uniquely identify tables across both input vectors.
        // - All source tables supplied are candidates for this planning pass;
        //   this method does not choose a subset based on table size or age.
        // - File age and duplicate-key precedence are not considered here.

        // Returns connected groups of overlapping ranges that contain at
        // least one source table; groups containing only destination tables
        // are omitted.
        static std::vector<CompactionPlan> planOverlappingGroups(
            const std::vector<SSTableMetadata> &sourceTables,
            const std::vector<SSTableMetadata> &destinationTables);
    };
}

#endif
