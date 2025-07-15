// This header file defines the DB interface.
#ifndef DB_H
#define DB_H

#include <iostream>

// TODO: make this into an interface or something
// This will be a virtual class
class DB
{
public:
    virtual std::string put(std::string key, std::string val) = 0;

    virtual std::string get(std::string key) = 0;

    virtual std::string del(std::string key) = 0;
};

#endif