// this class is the middle guy between frontend layers (eg. CLI, TCP server) and our core DB engine (exposed through DbImpl).
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <vector>
#include "core/db.h"
#include "core/sstable_manager/sstable_manager_impl.h"

class CommandHandler
{
public:
    CommandHandler(DB &db, SSTableManager &ssTableManager) : db_{db}, ssTableManager_{ssTableManager} {};

    // executes a given command
    Response<std::string> execute(const std::string &cmd, const std::vector<std::string> &args);

private:
    DB &db_;
    SSTableManager &ssTableManager_;
};

#endif