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
    bool decodeLine(const std::string &line, protocol::Request &outReq, std::string &err);
    std::string encodeResponse(const protocol::Response &resp); // "OK <message>\n" / "ERR <message>\n"
}

#endif