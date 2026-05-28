// this class is the middle guy between frontend layers (eg. CLI, TCP server) and our core DB engine (exposed through DbImpl).
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <vector>
#include "core/db.h"
#include "core/storage_manager/storage_manager_impl.h"
#include "api/message.h"

// This class handles commands in the form of (cmd, args).
// It is the API through which users communicate with the system.
class CommandHandler
{
public:
    CommandHandler(DB &db) : db_{db} {};

    // executes a given command
    protocol::Response execute(const protocol::Command &cmd, const std::vector<std::string> &args);

private:
    DB &db_;
};

#endif