#include <iostream> // std::cout
#include <sstream>  // std::istringstream
#include "core/db_impl.h"
#include "mem_table/mem_table_impl.h"
#include "skip_list/skip_list.h"

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
    // DiskManagerImpl diskManagerImpl("tinyDBstore.txt");
    // WriteBufferImpl writeBufferImpl(2, diskManagerImpl);
    // DbImpl dbImpl(writeBufferImpl, diskManagerImpl); // Creates the object on the stack. Destroyed once `main` function returns.
    // MemTable memTable(50);
    SkipList skiplist;
    MemTableImpl memTableImpl(1, skiplist);
    DbImpl dbImpl(memTableImpl);

    std::cout << "Welcome to TinyKV! Type PUT, GET, DEL or EXIT. \n";
    std::string line; // variable `line` that stores a (dynamically resized) string

    while (true)
    {
        std::cout << "> ";
        std::getline(std::cin, line); // stores input inside variable `line`
        std::istringstream iss(line); // instantiate a `std::istringstream` instance with the stream `line`

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
            // dbImpl.put(key, val);
            memTableImpl.put(key, val);
            break;

        case Command::GET:
            if (key.empty())
            {
                std::cout << "Usage: GET <key>"
                             "\n";
                break;
            }
            // dbImpl.get(key);
            memTableImpl.get(key);
            break;

        case Command::DEL:
            if (key.empty())
            {
                std::cout << "Usage: DEL <key>"
                             "\n";
                break;
            }
            // dbImpl.del(key);
            memTableImpl.del(key);
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