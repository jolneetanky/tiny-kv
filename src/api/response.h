#ifndef API_RESPONSE_H
#define API_RESPONSE_H

#include <iostream>

// Shape of our DB response, think of it like what's exposed to the frontend
// Generic version, for responses with data
template <typename T>
struct Response
{
    bool success;
    std::string message;
    std::optional<T> data; // Eg. for GET response

    Response(bool success = true, std::string message = "", std::optional<T> val = std::nullopt) : success{success}, message{std::move(message)}, data{std::move(val)} {};
};

// For responses with no data
template <> // Signify an EXPLICIT template specialization
struct Response<void>
{
    bool success;
    std::string message;

    Response(bool success = true, std::string message = "") : success{success}, message{std::move(message)} {};
};

#endif