#ifndef NET_FD_CONNECTION_H
#define NET_FD_CONNECTION_H

#include "net/connection.h"
#include <string>

// Handles:
// 1. Reading a line from a TCP byte stream, up to "\n"
// 2. Send all bytes of a given string over a TCP connection.
class FdConnection : public Connection
{
public:
    explicit FdConnection(int fd); // prevent an `int` from being implicitly converted to a `FdConnection`
    std::optional<std::string> readLine() override; // returns nullopt on EOF/error
    bool writeAll(const std::string &data) override; // returns false on error
    void close() override;

private:
    int fd_;
    std::string buffer_; // buffer partial reads from TCP stream
};

#endif
