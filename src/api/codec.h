#ifndef CODEC_H
#define CODEC_H

#include "message.h"
#include <string>

//
namespace protocol
{
    // Start simple: one line command, newline-terminated:
    // "GET key\n", "PUT key value\n"
    static Command parseCommand(std::string cmd); // static free function gives internal linkage. Every cpp file that defines `parseCommand()` has its own copy of the function. Be asue this is a private helper function only used by `codec.cpp`.
    // string -> Request
    bool decodeLine(const std::string &line, protocol::Request &outReq, std::string &err);
    // Request -> string
    std::string encodeRequest(const protocol::Request &req);

    // Response -> string
    std::string encodeResponse(const protocol::Response &resp); // "OK <message>\n" / "ERR <message>\n"
    // string -> Response
    bool decodeResponseLine(const std::string &line, protocol::Response &outRes, std::string &err);
}

#endif