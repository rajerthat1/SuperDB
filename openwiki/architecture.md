# Architecture

key_val uses a single-threaded, event-driven architecture built on Linux `epoll`. The design follows four layers, each with a clear responsibility.

## Layer diagram

```
┌─────────────────────────────────────────────────┐
│                   main.cpp                       │
│  InMemoryEngine → WalEngine → Server(port, wal) │
└───────────────────┬─────────────────────────────┘
                    │
┌───────────────────▼─────────────────────────────┐
│  Server (epoll event loop)                       │
│  /src/server/server.h + server.cpp              │
│                                                  │
│  - listen_fd: accepts new connections            │
│  - signalfd: catches SIGINT/SIGTERM for cleanup  │
│  - unordered_map<int, Connection>: active conns  │
│  - runs event loop until signal received         │
└───────────────────┬─────────────────────────────┘
                    │ per-connection
┌───────────────────▼─────────────────────────────┐
│  Connection (state machine)                      │
│  /src/server/connection.h + connection.cpp      │
│                                                  │
│  - RESPReader parser: incremental, buffer-safe   │
│  - write_buf: outgoing reply buffer              │
│  - close_after_flush: marks connection for close │
│  - Calls handle_command() for each parsed cmd    │
└───────────────────┬─────────────────────────────┘
                    │
┌───────────────────▼─────────────────────────────┐
│  Command Handler                                 │
│  /src/commands/handler.h + handler.cpp          │
│                                                  │
│  - Case-insensitive dispatch (to_upper)          │
│  - PING, SET, GET, DEL, EXISTS                   │
│  - Returns Reply variant                         │
└───────────────────┬─────────────────────────────┘
                    │
┌───────────────────▼─────────────────────────────┐
│  Storage Layer (IStorageEngine interface)        │
│  /src/storage/storage.h                          │
│                                                  │
│  ├── InMemoryEngine (unordered_map + shared_mtx) │
│  └── WalEngine (decorator, logs before write)    │
└─────────────────────────────────────────────────┘
```

Source: `/src/main.cpp` (lines 6-14)

## Server — epoll event loop

`Server::run()` in `/src/server/server.cpp` (lines 18-117) implements the core event loop:

1. **Socket setup** — Creates a non-blocking TCP socket, binds to `0.0.0.0:6379`, starts listening (backlog 128). Uses `SO_REUSEADDR`.
2. **Signal handling** — Blocks `SIGINT` and `SIGTERM` so default handlers don't run, then creates a `signalfd` that becomes readable when these signals arrive. The signalfd goes into epoll, making the event loop wake up naturally.
3. **epoll setup** — Creates an epoll instance with `epoll_create1(0)`. Registers the listen socket and the signalfd with `EPOLLIN`.
4. **Event loop** (lines 56-108):
   - `epoll_wait()` blocks until events arrive
   - If the signalfd is readable, sets `running = false` and breaks
   - If the listen fd is readable, calls `accept4()` in a loop to drain the accept queue; each new connection gets `EPOLLIN | EPOLLOUT` events and a fresh `Connection` object
   - For client fd events: reads into a 64KB stack buffer, feeds data to `Connection::on_readable()`, then calls `Connection::on_writable()` to flush responses
5. **Graceful shutdown** (lines 110-117): Closes all client connections, then cleans up server fds.

### Why single-threaded?

Redis itself uses a single-threaded event loop for command execution. This eliminates concurrency bugs in the command path. The storage engine uses `shared_mutex` internally only for the in-memory hash map — this is a defensive measure since all access is from the single event loop thread.

Source: `/src/server/server.cpp`

## Connection — per-client state

`Connection` (in `/src/server/connection.h`) is a struct holding:

- **`RESPReader parser`** — Incremental RESP protocol parser
- **`std::string write_buf`** — Buffer of serialized replies to send
- **`bool close_after_flush`** — Set to true when a protocol error occurs; the connection is closed after pending data is written

### Read path (`on_readable` in `/src/server/connection.cpp`, lines 10-35)

```
feed(data) → while try_parse() → handle_command() → serialize → append to write_buf
```

After parsing, the method checks two protocol error conditions:

1. **Garbage detection** — If the buffer has data but doesn't start with `*` (RESP array), it sends an error and sets `close_after_flush`.
2. **Size limit** — If the buffer exceeds 256KB (`kMaxReadBuf`), it sends an error and sets `close_after_flush`.

### Write path (`on_writable` in `/src/server/connection.cpp`, lines 37-47)

Writes `write_buf` to the fd using `write()`. Handles partial writes by erasing only the sent bytes. On errors other than `EAGAIN`/`EWOULDBLOCK`, the connection is marked for close.

## Graceful shutdown with signalfd

Traditional signal handling in event loops is fragile — signal handlers run in a separate execution context and can't safely modify heap objects. key_val uses `signalfd` (Linux-specific) to convert signals into fd-readable events:

```cpp
// /src/server/server.cpp, lines 35-41
sigset_t sig_mask;
sigemptyset(&sig_mask);
sigaddset(&sig_mask, SIGINT);
sigaddset(&sig_mask, SIGTERM);
sigprocmask(SIG_BLOCK, &sig_mask, nullptr);
int shutdown_fd = signalfd(-1, &sig_mask, SFD_NONBLOCK);
```

The signalfd is registered with epoll. When `SIGINT` or `SIGTERM` arrives, the event loop wakes up, reads the signal info, sets `running = false`, and breaks. After the loop, all connections are drained and closed.

## Change guidance

When modifying the event loop or connection handling:

1. **Test with redis-cli** — Run `./build/kv-server` and run `redis-cli PING`, SET/GET/DEL/EXISTS, and pipelined commands.
2. **Verify graceful shutdown** — Send SIGINT (Ctrl+C) while connections are active and confirm clean exit without leaks.
3. **Run storage tests** — `./build/tests/test_storage` covers the WAL replay path which is exercised on every server restart.
4. **Watch buffer sizes** — The 64KB read buffer and 256KB parser limit are safety boundaries. If you change them, update `kMaxReadBuf` in `connection.h` and verify with large requests.

## See also

- [Protocol & Command Handling](protocol.md) — RESP parser, serializer, and command dispatch
- [Storage Layer](storage.md) — InMemoryEngine, WalEngine, crash recovery
- [Operations & Testing](operations.md) — Build, run, test, and development notes
