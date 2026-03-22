#ifndef NET_SESSION_H
#define NET_SESSION_H

#include "api/command_handler.h"
#include "net/connection.h"

class Session
{
public:
    Session(Connection &conn, CommandHandler &handler);
    void run(); // starts an infinite loop that listens for requests from a particular Connection, handles the request, and sends the response back to the Connection.

private:
    Connection &conn_;
    CommandHandler &handler_;
};

#endif
