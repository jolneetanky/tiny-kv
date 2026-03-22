#include "net/session.h"

#include "api/codec.h"

Session::Session(Connection &conn, CommandHandler &handler) : conn_{conn}, handler_{handler} {}

void Session::run()
{
    while (true)
    {
        const auto line = conn_.readLine();
        if (!line)
        {
            break;
        }

        protocol::Request req;
        std::string parseErr;
        if (!protocol::decodeLine(line.value(), req, parseErr))
        {
            conn_.writeAll(protocol::encodeResponse({false, parseErr}));
            continue;
        }

        // Send request to commandHandler
        protocol::Response res = handler_.execute(req.command, req.args);
        conn_.writeAll(protocol::encodeResponse(res));

        // Break from while loop if user exits
        if (req.command == protocol::Command::EXIT)
        {
            break;
        }
    }

    conn_.close();
}
