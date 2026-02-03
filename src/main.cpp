#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

#include "factories/db_factory.h"
#include "core/db_impl.h"

enum class Command
{
    PUT,
    GET,
    DEL,
    EXIT,
    _FLUSH, // admin command
    UNKNOWN,
};

static Command parseCommand(std::string cmd)
{
    std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                   [](unsigned char c)
                   { return static_cast<char>(std::toupper(c)); });
    if (cmd == "PUT")
        return Command::PUT;
    if (cmd == "GET")
        return Command::GET;
    if (cmd == "DEL")
        return Command::DEL;
    if (cmd == "EXIT")
        return Command::EXIT;
    if (cmd == "_FLUSH")
        return Command::_FLUSH;
    return Command::UNKNOWN;
}

int main()
{
    std::unique_ptr<DbImpl> db = DbFactory::createDbForTests();

    std::cout << "Welcome to TinyKV! Commands: PUT <key> <value>, GET <key>, DEL <key>, EXIT\n";
    std::string line;

    while (true)
    {
        std::cout << "> ";
        if (!std::getline(std::cin, line))
        {
            std::cout << "\n";
            return 0;
        }

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd.empty())
        {
            continue;
        }

        Command parsedCmd = parseCommand(cmd);
        switch (parsedCmd)
        {
        case Command::PUT:
        {
            std::string key;
            iss >> key;
            std::string value;
            std::getline(iss, value);
            if (!value.empty() && value.front() == ' ')
            {
                value.erase(0, value.find_first_not_of(' '));
            }
            if (key.empty() || value.empty())
            {
                std::cout << "Usage: PUT <key> <value>\n";
                break;
            }
            Response<void> resp = db->put(key, value);
            std::cout << (resp.success ? "OK" : "ERR") << (resp.message.empty() ? "" : (": " + resp.message)) << "\n";
            break;
        }
        case Command::GET:
        {
            std::string key;
            iss >> key;
            if (key.empty())
            {
                std::cout << "Usage: GET <key>\n";
                break;
            }
            Response<std::string> resp = db->get(key);
            if (resp.success && resp.data.has_value())
            {
                std::cout << *resp.data << "\n";
            }
            else
            {
                std::cout << (resp.message.empty() ? "NOT_FOUND" : resp.message) << "\n";
            }
            break;
        }
        case Command::DEL:
        {
            std::string key;
            iss >> key;
            if (key.empty())
            {
                std::cout << "Usage: DEL <key>\n";
                break;
            }
            Response<void> resp = db->del(key);
            std::cout << (resp.success ? "OK" : "ERR") << (resp.message.empty() ? "" : (": " + resp.message)) << "\n";
            break;
        }
        case Command::_FLUSH:
        {
            Response<void> resp = db->forceFlushForTests();
            break;
        }
        case Command::EXIT:
            return 0;
        default:
            std::cout << "Unknown command. Use PUT, GET, DEL, or EXIT.\n";
            break;
        }
    }
}
//