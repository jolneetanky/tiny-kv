#include <iostream> // std::cout
#include <sstream>  // std::istringstream

#include "api/command_handler.h"
#include "factories/db_factory.h"

// Define a CommandHandler to handle commands? I'm not sure

// TODO: instead of CLI, make it a networked interface for easier e2e testing for expected behavior.
// Main program to interact with our DB, for testing

// CLI does a few things:
// 1. Read user input
// 2. Tokenize user input, split into command and args
// 3. Pass that to CommandHandler to execute, or return error
// 4. Print data.

int main()
{
    std::unique_ptr<DB> db = DbFactory::createDefaultDb(); // destroyed when main ends
    CommandHandler handler(*db);

    return 0;
}