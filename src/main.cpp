#include <iostream> // std::cout
#include <sstream>  // std::istringstream
#include "core/db_impl.h"
#include "mem_table/mem_table_impl.h"
#include "skip_list/skip_list_impl.h"
#include "sstable_manager/sstable_manager_impl.h"
#include "wal/wal.h"

// contexts
#include "contexts/system_context.h"

// factories
#include "factories/bloom_filter_factory.h"

// TODO: instead of CLI, make it a networked interface for easier e2e testing for expected behavior.
enum class Command
{
    PUT,
    GET,
    DEL,
    EXIT,
    UNKNOWN,
    COMPACT,
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
    if (cmd == "COMPACT" || cmd == "compact")
        return Command::COMPACT;
    return Command::UNKNOWN;
}

int main()
{
    // initialize factories
    DefaultBloomFilterFactory bff; // we can have a factory for mocks as well

    // pass in system context
    SystemContext systemCtx(bff); 

    // initialize classes
    SSTableManagerImpl ssTableManagerImpl(systemCtx);
    SkipListImpl skipListImpl;
    WAL wal(0);
    MemTableImpl memTableImpl(3, skipListImpl, ssTableManagerImpl, wal, systemCtx);
    DbImpl dbImpl(memTableImpl, ssTableManagerImpl); // this is the main class tbh that depends on the classes above

    ssTableManagerImpl.initLevels();
    memTableImpl.replayWal();

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
            dbImpl.put(key, val);
            // memTableImpl.put(key, val);
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

        case Command::COMPACT:
            ssTableManagerImpl.compact();
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