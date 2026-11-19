#include "core/level_manager/compaction_planner.h"

#include <algorithm>
#include <utility>

namespace tinykv
{
    namespace
    {
        struct TableRange
        {
            bool isSource;
            FileNumber fileNumber;
            const SSTableMetadata *metadata;
        };

        void sortFileNumbers(std::vector<FileNumber> &fileNumbers)
        {
            std::sort(fileNumbers.begin(), fileNumbers.end());
        }

        void appendToPlan(CompactionPlan &plan, const TableRange &range)
        {
            if (range.isSource)
                plan.sourceFileNumbers.push_back(range.fileNumber);
            else
                plan.destinationFileNumbers.push_back(range.fileNumber);

            if (range.metadata->m_max_key > plan.maxKey)
                plan.maxKey = range.metadata->m_max_key;
        }

        void finishPlan(std::vector<CompactionPlan> &plans, CompactionPlan &plan)
        {
            if (plan.sourceFileNumbers.empty())
                return;

            sortFileNumbers(plan.sourceFileNumbers);
            sortFileNumbers(plan.destinationFileNumbers);
            plans.emplace_back(std::move(plan));
        }
    }

    std::vector<CompactionPlan> CompactionPlanner::planOverlappingGroups(
        const std::vector<SSTableMetadata> &sourceTables,
        const std::vector<SSTableMetadata> &destinationTables)
    {
        std::vector<TableRange> ranges;
        ranges.reserve(sourceTables.size() + destinationTables.size());

        for (std::size_t i = 0; i < sourceTables.size(); ++i)
            ranges.push_back(TableRange{true, sourceTables[i].m_file_number, &sourceTables[i]});

        for (std::size_t i = 0; i < destinationTables.size(); ++i)
            ranges.push_back(TableRange{false, destinationTables[i].m_file_number, &destinationTables[i]});

        // sort all tables by (min_key, max_key) ranges to get the intervals
        std::sort(ranges.begin(), ranges.end(), [](const TableRange &left, const TableRange &right)
                  {
                      if (left.metadata->m_min_key != right.metadata->m_min_key)
                          return left.metadata->m_min_key < right.metadata->m_min_key;
                      if (left.metadata->m_max_key != right.metadata->m_max_key)
                          return left.metadata->m_max_key < right.metadata->m_max_key;

                        //  TODO: remove the below `isSource` and fileNumber` checks
                        // these are redundnat
                        // because only minkey and maxkey intervals matter right now.
                        // as long as minkey and maxkey overlap, the files will be placed in the same group
                        // fileNumber and `isSource` doesn't matter rn.
                      if (left.isSource != right.isSource)
                          return left.isSource;
                      return left.fileNumber < right.fileNumber; });

        std::vector<CompactionPlan> plans;
        if (ranges.empty())
            return plans;

        CompactionPlan currentPlan;
        bool hasCurrentPlan = false;

        for (const TableRange &range : ranges)
        {
            // end the previous interval (if any) and start a new interval
            if (!hasCurrentPlan || range.metadata->m_min_key > currentPlan.maxKey)
            {
                if (hasCurrentPlan)
                    finishPlan(plans, currentPlan);

                currentPlan = CompactionPlan{};
                currentPlan.minKey = range.metadata->m_min_key;
                currentPlan.maxKey = range.metadata->m_max_key;
                hasCurrentPlan = true;
            }

            appendToPlan(currentPlan, range);
        }

        if (hasCurrentPlan)
            finishPlan(plans, currentPlan);

        return plans;
    }
}
