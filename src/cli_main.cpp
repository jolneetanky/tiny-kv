#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <optional>

#include "cli/repl.h"
#include "net/fd_connection.h"

std::optional<uint16_t> readPortFromEnv()
{
    const char *raw = std::getenv("TINYKV_PORT");
    if (raw == nullptr || raw[0] == '\0')
    {
        return std::nullopt;
    }

    char *end = nullptr;
    errno = 0;
    const unsigned long parsed = std::strtoul(raw, &end, 10);
    if (errno != 0 || end == raw || *end != '\0' || parsed > 65535)
    {
        std::cerr << "Invalid TINYKV_PORT: " << raw << "\n";
        return std::nullopt;
    }

    return static_cast<uint16_t>(parsed);
}

// Create a socket, and use that socket to connect to `host` IP address
int connectToServer(const char *host, uint16_t port)
{
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        std::cerr << "Failed to create socket: " << std::strerror(errno) << "\n";
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    // convert string host number ("127.0.0.0") to int
    if (::inet_pton(AF_INET, host, &addr.sin_addr) != 1)
    {
        std::cerr << "Invalid server address: " << host << "\n";
        ::close(fd);
        return -1;
    }

    if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        std::cerr << "Failed to connect to " << host << ":" << port
                  << ": " << std::strerror(errno) << "\n";
        ::close(fd);
        return -1;
    }

    return fd;
}

// this client program:
// 1. connects to tinyKV server via TCP
// 2. runs a REPL that
// - reads client commands, encodes it and sends to server over the connection
// - reads commands from tinyKV server, and prints it on the client's terminal.
int main()
{
    constexpr const char *kDefaultHost = "127.0.0.1";
    constexpr uint16_t kDefaultPort = 6379;

    const uint16_t port = readPortFromEnv().value_or(kDefaultPort);
    const int fd = connectToServer(kDefaultHost, port);
    if (fd < 0)
    {
        return 1;
    }

    std::cout << "Connected to tinykv at " << kDefaultHost << ":" << port << "\n";

    FdConnection conn(fd);
    return runRepl(conn);
}