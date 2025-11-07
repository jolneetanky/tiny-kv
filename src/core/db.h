// This header file defines the DB interface.
#ifndef DB_H
#define DB_H

#include <iostream>

// Shape of our DB response, think of it like what's exposed to the frontend
// Generic version, for responses with data
template <typename T>
struct Response
{
    bool success;
    std::string message;
    std::optional<T> data; // Eg. for GET response

    Response(bool success = true, std::string message = "", std::optional<T> val = std::nullopt) : success{success}, message{std::move(message)}, data{std::move(data)} {};
};

// For responses with no data
template <> // Signify an EXPLICIT template specialization
struct Response<void>
{
    bool success;
    std::string message;

    Response(bool success = true, std::string message = "") : success{success}, message{std::move(message)} {};
};

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