#include "net/connection.h"

// 1. Just loops, takes in user input (via std::cin)
// 2. Sends that to server via `conn.writeAll(line)`
// 3. Listens for server response via `res = conn.readline()
// 4. Decodes response, and prints result accordingly to the terminal (by writing to `std::cout`).
int runRepl(Connection &conn);