#ifndef API_MESSAGE_H
#define API_MESSAGE_H

#include <string>
#include <vector>
#include <optional>

namespace protocol
{
    enum class Command
    {
        PUT,
        GET,
        DEL,
        EXIT,
        _FLUSH, // admin command to manually trigger flush to disk (otherwise, flushes happen when memtable gets full)
        UNKNOWN,
    };

    struct Request
    {
        Command command;
        std::vector<std::string> args;
    };

    struct Response
    {
        bool ok = false;
        std::optional<std::string> data;
    };
} // namespace protocol

#endif
