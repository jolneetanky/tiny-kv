Main problem: how will the server listen for client connections via TCP, and handle each request accordingly?

## 1) Protocol and Codecs

The protocol defines the shape of each Request and Response.

```cpp
// src/api/message.h
namespace protocol
{
    enum class Command
    {
        PUT,
        GET,
        DEL,
        EXIT,
        _FLUSH, // admin command to manually trigger flush to disk (otherwise, flushes happen when memtable gets full)
        UNKNOWN,
    };

    struct Request
    {
        Command command;
        std::vector<std::string> args;
    };

    struct Response
    {
        bool ok = false;
        std::optional<std::string> data;
    };
}
```

Notes:

- `Response` should probably not be templated at call sites for v1. Keep it concrete:
  - `bool success`
  - `std::optional<T> data` - currently stores either the data itself (eg. for GET requests), or the error message if any. This can lead to some ambiguity.
- Keep protocol text-based initially for easy testing via `nc`/Python.

### 1.1) Codec

- Codec encodes/decodes a Request/Response into strings, to be transmitted across the wire.
- The strings are newline-delimited (ie. newline-terminated) text like `GET key\n`, `OK value\n`, `ERR message\n`

#### 1.1.1) Request encoding/decoding

##### String representation

General shape: `<COMMAND> <arg0> <arg1> ...\n`

1. `GET <key>\n`
2. `PUT <key> <val>\n`
3. `DEL <key>\n`

##### C++ struct

##### Implementation

- Refer to `src/api/codec.h` and `src/api/codec.cpp` for implementation details.

#### 1.1.2) Response encoding/decoding

##### String representation

- Previous String represenation: `<STATUS> <data>\n` (Eg. `OK world\n`, `ERR key does not exist\n`, etc.)
- (TODO) Reformat to `<STATUS> <message_len> <message> <data_len> <data>\n`
- Examples:

```
status=OK message=2:OK data=5:world\n
status=OK message=16:Successfully PUT data=null\n
status=ERR message=18:Key does not exist data=null\n
```

##### C++ struct

```cpp
struct Response
{
bool ok = false;
std::string message;
std::optional<std::string> data;
};
```

##### Implementation

- Refer to `src/api/codec.h` and `src/api/codec.cpp` for implementation details.

## 2) Approaches

### APPROACH #1: Single-Threaded TCP Server + Event Loop

`Session.run()` does:

1. Spins up a TCP listening server.
2. Server handles 2 types of requests - 1) connection requests, 2) Requests from an existing connection.
3. Use kqueue/epoll to listen for events from connected fds and handle them accordingly.
   QN: how can I do 2 and 3 at the same time if `listen()` is blocking?

Answer:

- `listen()` itself does not block; it marks the socket as passive (ready to accept).
- `accept()` is the blocking call.
- In an event loop design, you:
  1. Create listening socket.
  2. Set it non-blocking.
  3. Register it with `kqueue/epoll`.
  4. When it becomes readable, that means one or more pending inbound connections; call `accept()` in a loop until EAGAIN.
- _The same event loop monitors both the listening socket and all client sockets._

Pros:

- Very scalable, one thread can handle many idle/slow clients.
- No per-connection thread overhead.

Cons:

- Hardest implementation complexity (state machines, partial reads/writes, edge-trigger behavior).
- Not needed for first milestone unless you want concurrency from day one.

### APPROACH #2: Per-Connection Thread Handler

`Session.run()` does:

1. Spins up a TCP listening server.
2. Listens for incoming connections, spawns a worker thread to handle it.
3. Worker will listen on that fd for bytes.
4. Worker decodes bytes into a Request, via `request = codec::decodeLine(line)`
5. Response res = RequestHandler(req)
6. Codec::encodeResopnse(res)
7. Send Response to clients.

// TODO (for you @ cursor): Write an example code snippet to illustrate how this looks like.

Example skeleton:

```cpp
int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
// bind/listen omitted for brevity

while (true) {
    int client_fd = ::accept(listen_fd, nullptr, nullptr); // blocks until a client arrives
    if (client_fd < 0) continue;

    std::thread([client_fd, &handler] {
        FdConnection conn(client_fd);
        Session session(conn, handler);
        session.run();     // read -> decode -> execute -> encode -> write
        conn.close();
    }).detach();
}
```

Pros:

- Very straightforward mental model.
- Fastest way to get working server + REPL + Python integration tests.

Cons:

- Unbounded thread growth under many clients.
- Context-switch overhead and memory per thread.
- Need thread-safety around DB if you process commands concurrently.

### APPROACH #3: Abstracted Connection + Session

Similar to approach 2, but with more abstraction layers.

- One session per connection
- Each session runs on a separate thread.
- Main thread listens for connection requests, accepts them, and creates a new Session to handle the request.
- `Session` should not care whether transport is TCP or UDS. It only uses `Connection`.
- Main accept loop chooses transport and session lifecycle strategy (single-thread, thread-per-conn, pool, evented).

Main flow for one request-response loop per session:

1. On the highest level, we call `Session.run()`. This starts an infinite loop that listens for requests from a particular connection.
2. Read requests via `line = connection.readLine()`.
3. Decode the line via `Request req = codec::decodeLine(line)`.
4. Response res = RequestHandler(req)
5. Codec::encodeResopnse(res)
6. Write request to connections vis `connection.writeAll()`

#### `Connection` class

- Connection::readLine() - Reads bytes written into the connection's Socket.
- Connection::writeAll() - Keep calling `write()` until every byte in `data` has been sent, or return `false` on fatal error / closed peer.
- Connection::close() - Close the connection. This means the server will no longer accept requests from this connection, and no longer send responses to this connection.

```cpp
#pragma once
#include <optional>
#include <string>

class Connection {
public:
    virtual ~Connection() = default;
    virtual std::optional<std::string> readLine() = 0;  // returns nullopt on EOF/error
    virtual bool writeAll(const std::string& data) = 0;
    virtual void close() = 0;
};
```

```cpp
#pragma once
#include "connection.h"

class FdConnection : public Connection {
public:
    explicit FdConnection(int fd);
    std::optional<std::string> readLine() override;
    bool writeAll(const std::string& data) override;
    void close() override;
private:
    int fd_;
    std::string buffer_;
};
```

Implementation note:

- Use `recv()`/`send()` or `read()`/`write()` consistently.
- Prefer looped `send` for `writeAll`.
- For future multi-threading, ensure no concurrent writers on the same socket unless externally synchronized.

#### `Session`

- Session::run() - Starts a while loop that listens for incoming requests to a particular Connection, handles the request, and sends the response back to the Connection.

```cpp
#pragma once
#include "net/connection.h"
#include "api/command_handler.h"

class Session {
public:
    Session(Connection& conn, CommandHandler& handler) : conn_(conn), handler_(handler) {}
    void run();
private:
    Connection& conn_;
    CommandHandler& handler_;
};
```
