#include "api/command_handler.h"

Response<std::string> CommandHandler::execute(
    const std::string &cmd, const std::vector<std::string> &args)
{
    std::string lowerCmd = cmd;
    std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), ::tolower);

    if (lowerCmd == "put")
    {
        if (args.size() < 2)
            return {false, "Usage: PUT <key> <value>"};
        auto res = db_.put(args[0], args[1]);
        if (res.success)
        {
            return {res.success, "OK"};
        }
        return {res.success, res.message};
    }

    if (lowerCmd == "get")
    {
        if (args.empty())
            return {false, "Usage: GET <key>"};
        return db_.get(args[0]);
    }

    if (lowerCmd == "del")
    {
        if (args.empty())
            return {false, "Usage: DEL <key>"};
        auto res = db_.del(args[0]);
        if (res.success)
        {
            return {res.success, "OK"};
        }
        return {res.success, res.message};
    }

    return {false, "Unknown command"};
}
