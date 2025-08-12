#ifndef WRITE_AHEAD_LOG
#define WRITE_AHEAD_LOG

#include <optional>
#include <vector>
#include "../types/error.h"
#include "../types/entry.h"

class WAL{
    private:
        std::string BASE_PATH = "./wal";
        uint64_t m_id;
        std::string m_fname;
        std::string m_fullPath;

        std::string _buildFileName(uint64_t id);

    public:
        WAL(uint64_t id);
        std::optional<Error> append(const Entry& entry);
        std::optional<Error> remove();
        std::optional<std::vector<Entry>> getEntries();
};

#endif