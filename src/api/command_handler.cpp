#include "api/command_handler.h"

protocol::Response CommandHandler::execute(
    const protocol::Command &cmd, const std::vector<std::string> &args)
{
    switch (cmd)
    {
    case protocol::Command::GET:
    {
        if (args.empty())
            return {false, "Usage: GET <key>", std::nullopt};
        return db_.get(args[0]);
    }

    case protocol::Command::PUT:
    {
        if (args.size() < 2)
            return {false, "Usage: PUT <key> <value>", std::nullopt};
        return db_.put(args[0], args[1]);
    }

    case protocol::Command::DEL:
    {
        if (args.empty())
            return {false, "Usage: DEL <key>", std::nullopt};

        return db_.del(args[0]);
    }

    case protocol::Command::_FLUSH:
    {
        return db_.forceFlushForTests();
    }

    case protocol::Command::EXIT:
        return {true, "Bye", std::nullopt};

    default:
        return {false, "Unknown command", std::nullopt};
        break;
    }
}
