#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <thread>

#include "api/command_handler.h"
#include "core/db.h"
#include "factories/db_factory.h"
#include "net/fd_connection.h"
#include "net/session.h"

int createListenSocket(uint16_t port)
{
    int listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0)
    {
        std::cerr << "Failed to create listening socket: socket creation failed" << "\n";
        return -1;
    }

    const int yes = 1;
    ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (::bind(listenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        const int bindErrno = errno;
        std::cerr << "Failed to create listening socket: bind error: "
                  << std::strerror(bindErrno) << " (errno " << bindErrno << ")" << "\n";
        if (bindErrno == EADDRINUSE)
        {
            std::cerr << "Port " << port
                      << " is already in use. Set TINYKV_PORT to a different port or stop the existing listener." << "\n";
        }
        ::close(listenFd);
        return -1;
    }

    if (::listen(listenFd, 64) < 0)
    {
        std::cerr << "Failed to create listening socket: failed to mark listen" << "\n";
        ::close(listenFd);
        return -1;
    }

    return listenFd;
}

std::optional<uint16_t> readPortFromEnv()
{
    const char *rawPort = std::getenv("TINYKV_PORT");
    if (rawPort == nullptr || rawPort[0] == '\0')
    {
        return std::nullopt;
    }

    char *end = nullptr;
    errno = 0;
    const unsigned long parsed = std::strtoul(rawPort, &end, 10);
    if (errno != 0 || end == rawPort || *end != '\0' || parsed > std::numeric_limits<uint16_t>::max())
    {
        std::cerr << "Invalid TINYKV_PORT value: " << rawPort << "\n";
        return std::nullopt;
    }

    return static_cast<uint16_t>(parsed);
}

// this program starts the tinyKV server,
// and listesn for client requests.
// when a client request comes in, create a separate Session for the client.
int main()
{
    constexpr uint16_t kDefaultPort = 6379;
    const uint16_t port = readPortFromEnv().value_or(kDefaultPort);
    const int listenFd = createListenSocket(port); // Bind listening socket to this port
    if (listenFd < 0)
    {
        std::cerr << "Failed to create listening socket on port " << port << "\n";
        return 1;
    }

    std::cout << "tinykv-server listening on 0.0.0.0:" << port << "\n";

    std::unique_ptr<DB> db = DbFactory::createDefaultDb(); // now everything in DB lives on the heap. Deallocated when this `unique_ptr<DB>` goes out of scope.
    CommandHandler handler(*db);

    while (true)
    {
        int clientFd = ::accept(listenFd, nullptr, nullptr); // blocks until a client req comes in
        if (clientFd < 0)
        {
            continue;
        }

        std::thread([clientFd, &handler]()
                    {
            FdConnection conn(clientFd); // Created in thread's stack
            Session session(conn, handler); // Created in thread's stack
            session.run(); })
            .detach();
    }
}
