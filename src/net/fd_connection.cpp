#include "net/fd_connection.h"

#include <sys/socket.h>
#include <unistd.h>

FdConnection::FdConnection(int fd) : fd_{fd} {}

// this function reads bytes coming into an `fd`, and returns the next newline
// EDGE CASE: if the incoming bytes never terminates, buffer just keeps growing indefinitely
// but we assume the sender is well-behaved and sends a reasonable number of bytes
// before ending with "\n".

// returns `nullopt` when `recv()` returns a value <= 0
// 0: peer disconnected
std::optional<std::string> FdConnection::readLine()
{
    while (true)
    {
        // if "\n" is found, stop `recv()`ing and return the line.
        const std::size_t newLinePos = buffer_.find('\n');
        if (newLinePos != std::string::npos)
        {
            std::string line = buffer_.substr(0, newLinePos); // extract all bytes up until "\n"
            buffer_.erase(0, newLinePos + 1);
            return line;
        }

        // Read into a char buffer, then append this char buffer onto the buffer we currently maintain
        char chunk[1024];                                       // `char` array, essentially a string
        const ssize_t n = ::recv(fd_, chunk, sizeof(chunk), 0); // `n` == number of bytes received
        if (n <= 0)
        {
            return std::nullopt;
        }
        buffer_.append(chunk, static_cast<std::size_t>(n)); // static_cast so compiler actually casts safely
    }
}

// this function writes everything in `data` to the `fd`
bool FdConnection::writeAll(const std::string &data)
{
    // Why a while loop?
    // BECAUSE:
    // Ensure OS actually sends everything.
    // Because we might have cases where eg. OS' send buffer gets full, OS doesn't send due to TCP flow control, etc.
    std::size_t sent = 0;
    while (sent < data.size())
    {
        const ssize_t n = ::send(fd_, data.data() + sent, data.size() - sent, 0); // n == number of bytes sent
        if (n <= 0)
        {
            return false;
        }

        sent += static_cast<std::size_t>(n);
    }
    return true;
}

void FdConnection::close()
{
    if (fd_ >= 0)
    {
        ::shutdown(fd_, SHUT_RDWR);
        ::close(fd_);
        fd_ = -1;
    }
}
