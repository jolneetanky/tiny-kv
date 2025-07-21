#ifndef ERROR_H
#define ERROR_H

#include <string>

struct Error{
    std::string error;
    Error(std::string msg) : error{msg} {};
};

#endif