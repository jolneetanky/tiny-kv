// This header file defines the DB interface.
#ifndef DB_H
#define DB_H

#include <iostream>
#include "api/message.h"

// This class is the external, client-facing API.
class DB
{
public:
    virtual protocol::Response put(std::string key, std::string val) = 0;

    virtual protocol::Response get(std::string key) const = 0;

    virtual protocol::Response del(std::string key) = 0;

    // for testing purposes
    virtual protocol::Response forceCompactForTests() = 0;
    virtual protocol::Response forceFlushForTests() = 0;

    virtual ~DB() = default;
};

#endif