// This header file defines the DB interface.
#ifndef DB_H
#define DB_H

#include <iostream>
#include "api/response.h"

// This class is the external, client-facing API.
class DB
{
public:
    virtual Response<void> put(std::string key, std::string val) = 0;

    virtual Response<std::string> get(std::string key) const = 0;

    virtual Response<void> del(std::string key) = 0;

    virtual ~DB() = default;
};

#endif