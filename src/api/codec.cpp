#include "api/codec.h"

#include <sstream>
#include <algorithm>
#include <cctype>

namespace protocol
{
    static Command parseCommand(std::string cmd)
    {
        std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                       [](unsigned char c)
                       {
                           // remember a `char` is just an integer
                           // Eg. 'F' = 70 in ASCII.
                           // std::toupper(c) takes in an `unsigned char` (0-255),
                           // and returns an `int`.
                           // explicitly convert the `int` -> `char` for safety.
                           return static_cast<char>(std::toupper(c));
                       });
        if (cmd == "PUT")
            return Command::PUT;
        if (cmd == "GET")
            return Command::GET;
        if (cmd == "DEL")
            return Command::DEL;
        if (cmd == "EXIT")
            return Command::EXIT;
        if (cmd == "_FLUSH")
            return Command::_FLUSH;
        return Command::UNKNOWN;
    }

    bool decodeLine(const std::string &line, Request &outReq, std::string &err)
    {
        std::istringstream iss(line); // handles tokenization, eg. "GET mykey" or "GET     mykey" -> ["GET", "mykey"]
        std::string cmdStr;

        // `iss` is a stream (ie. has a buffer and all that)
        // so we stream the first toekn
        // >> reads a token from the stream into `cmdStr`.
        if (!(iss >> cmdStr))
        {
            err = "empty command";
            return false;
        }

        outReq.command = parseCommand(cmdStr);

        outReq.args.clear();

        // read the next tokens from the stream.
        // these tokens are arguments.
        std::string token;
        while (iss >> token)
        {
            outReq.args.push_back(token);
        }

        err.clear();
        return true;
    }

    std::string encodeResponse(const Response &resp)
    {
        const std::string prefix = resp.ok ? "OK" : "ERR";
        if (resp.message.empty())
        {
            return prefix + "\n";
        }
        return prefix + " " + resp.message + "\n";
    }
} // namespace protocol
