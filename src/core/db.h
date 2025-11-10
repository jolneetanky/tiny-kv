// This header file defines the DB interface.
#ifndef DB_H
#define DB_H

#include <iostream>
#include "api/response.h"

// TODO: make this into an interface or something
// This will be a virtual class
class DB
{
public:
    virtual Response<void> put(std::string key, std::string val) = 0;

    virtual Response<std::string> get(std::string key) const = 0;

    virtual Response<void> del(std::string key) = 0;
};

#endif