#include "net/connection.h"

// client side code
// 1. loops, takes in user input (via std::cin)
// 2. sends that to server via `conn.writeAll(line)`
// 3. listens for server response via `res = conn.readline()
// 4. decodes response, and prints result accordingly to the terminal (by writing to `std::cout`).
int runRepl(Connection &conn);