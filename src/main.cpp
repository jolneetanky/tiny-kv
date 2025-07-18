#include <iostream> // std::cout
#include <sstream>  // std::istringstream
#include "core/db_impl.h"
#include "writebuffer/writebuffer_impl.h"
#include "disk_manager/disk_manager_impl.h"

enum class Command
{
    PUT,
    GET,
    DEL,
    EXIT,
    UNKNOWN,
};

Command parseCommand(const std::string &cmd)
{
    if (cmd == "PUT" || cmd == "put")
        return Command::PUT;
    if (cmd == "GET" || cmd == "get")
        return Command::GET;
    if (cmd == "DEL" || cmd == "del")
        return Command::DEL;
    if (cmd == "EXIT" || cmd == "exit")
        return Command::EXIT;
    return Command::UNKNOWN;
}

int main()
{
    DiskManagerImpl diskManagerImpl("tinyDBstore.txt");
    WriteBufferImpl writeBufferImpl(2, diskManagerImpl);
    DbImpl dbImpl(writeBufferImpl, diskManagerImpl); // Creates the object on the stack. Destroyed once `main` function returns.

    std::cout << "Welcome to TinyKV! Type PUT, GET, DEL or EXIT. \n";
    std::string line; // variable `line` that stores a (dynamically resized) string

    while (true)
    {
        std::cout << "> ";
        std::getline(std::cin, line); // stores input inside variable `line`
        std::istringstream iss(line); // instantiate a `std::istringstream` instance with the stream `line`

        // std::cout << iss.str() << "\n"; // print out a copy of the current stream contents
        // extract cmd, key, value from stream (whitespace-delimited).
        std::string cmd;
        iss >> cmd;

        std::string key;
        iss >> key;

        std::string val;
        iss >> val;

        Command parsedCmd = {parseCommand(cmd)};

        switch (parsedCmd)
        {
        case Command::PUT:
            if (key.empty() || val.empty())
            {
                std::cout << "Usage: PUT <key> <value>" << "\n";
                break;
            }
            dbImpl.put(key, val);
            break;

        case Command::GET:
            if (key.empty())
            {
                std::cout << "Usage: GET <key>"
                             "\n";
                break;
            }
            dbImpl.get(key);
            break;

        case Command::DEL:
            if (key.empty())
            {
                std::cout << "Usage: DEL <key>"
                             "\n";
                break;
            }
            dbImpl.del(key);
            break;

        case Command::EXIT:
            return 0; // return early

        default:
            std::cout << "Unknown command. Use PUT, GET, DEL, or EXIT." << "\n";
            break;
        }
    }

    return 0;
}