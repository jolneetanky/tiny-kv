#ifndef NET_CONNECTION_H
#define NET_CONNECTION_H

#include <optional>
#include <string>

class Connection
{
public:
    virtual ~Connection() = default;
    virtual std::optional<std::string> readLine() = 0; // returns nullopt on EOF/error
    virtual bool writeAll(const std::string &data) = 0; // returns false on error
    virtual void close() = 0;
};

#endif