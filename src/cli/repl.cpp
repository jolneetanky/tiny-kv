#include "cli/repl.h"
#include "api/codec.h"
#include "api/message.h"

#include <iostream>

int runRepl(Connection &conn)
{
    std::string line; // buffers the current line read from std::cin

    while (true)
    {
        std::cout << "tinykv>> "; // flush output buffer
        std::cout.flush();

        if (!std::getline(std::cin, line))
        {
            std::cout << "\n";
            return 0;
        }

        if (line.empty())
            continue;

        protocol::Request req;
        std::string parseErr;

        // decode the line into a request.
        // this is necessary in case the line is some invalid input.
        if (!protocol::decodeLine(line, req, parseErr))
        {
            std::cerr << "ERR: " << parseErr << "\n";
            continue;
        }

        // Seems counterintuitive to decode and encode the same request again.
        const std::string encoded = protocol::encodeRequest(req);
        if (!conn.writeAll(encoded))
        {
            std::cerr << "ERR: failed to send request to server\n";
            return 1;
        }

        const auto rawRes = conn.readLine();
        if (!rawRes)
        {
            std::cerr << "ERR: Server disconnected\n";
            return 1;
        }

        // std::cout << rawRes.value() << "\n";

        // parse response
        protocol::Response res;
        std::string parseErr;
        if (!protocol::decodeResponseLine(rawRes.value(), res, parseErr))
        {
            std::cerr << "ERR: failed to decode server response: " << parseErr << "\n";
            return 1;
        }

        if (res.ok)
        {
            if (res.data.has_value())
            {
                std::cout << res.data.value() << "\n";
            }
            else
            {
                std::cout << "OK\n";
            }
        }
        else
        {
            std::cout << "ERR";
            if (res.data.has_value())
            {
                std::cout << ": " << res.data.value();
            }
            std::cout << "\n";
        }

        if (req.command == protocol::Command::EXIT)
        {
            return 0;
        }
    }
}
