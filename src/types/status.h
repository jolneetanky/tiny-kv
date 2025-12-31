#ifndef STATUS_H
#define STATUS_H

#include <string>

class Status
{
public:
    static Status OK() { return Status(Code::kOk, "OK"); }; // factory function
    static Status Error(std::string msg) { return Status(Code::kError, msg); };

    bool ok() const { return m_code == Code::kOk; }; // if !ok then it's an error
    std::string to_string() const { return m_msg; };

private:
    enum class Code
    {
        kOk = 0,
        kError = 1,
    };

    Status(Code code, std::string msg) : m_code{code}, m_msg{msg} {};

    Code m_code;
    std::string m_msg;
};

#endif