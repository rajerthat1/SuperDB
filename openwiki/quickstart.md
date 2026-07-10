# Key/Val — Redis-Compatible Key-Value Store

A Redis-compatible key-value store built from scratch in C++20. This project explores systems programming, storage engines, crash recovery, and distributed consensus by incrementally evolving a simple TCP key-value server into a production-grade storage system.

## Repository Overview

| Aspect | Detail |
|---|---|
| **Language** | C++20 |
| **Build** | CMake 3.20+ |
| **Tests** | Google Test (via FetchContent) |
| **Protocol** | RESP (Redis Serialization Protocol) |
| **Port** | 6379 (default) |

### Current Status

Complete through **Phase 1** (Core Reliability):

- **Phase 0** — Foundation: TCP server with epoll, RESP parser, in-memory storage, basic commands (PING, SET, GET, DEL, EXISTS)
- **Phase 1** — Core Reliability: Write-Ahead Log (WAL) with fsync durability, unit testing (36 tests), error handling, graceful shutdown via signalfd

Upcoming phases add LSM-tree storage, lock-free structures, SIMD parsing, Raft consensus, and more. See the [README roadmap](../README.md) for details.

## Quick Start

```bash
# Build
cmake -B build && cmake --build build

# Run
./build/kv-server

# Interact (from another terminal)
redis-cli PING
redis-cli SET mykey myvalue
redis-cli GET mykey
redis-cli EXISTS mykey
redis-cli DEL mykey
```

## Documentation Sections

| Page | Description |
|---|---|
| [Architecture](architecture.md) | Epoll event loop, connection state machine, graceful shutdown |
| [Protocol & Commands](protocol.md) | RESP parsing/serialization, command dispatch (PING, SET, GET, DEL, EXISTS) |
| [Storage Layer](storage.md) | IStorageEngine interface, InMemoryEngine, WalEngine (decorator pattern) |
| [Operations & Testing](operations.md) | Build, run, test, benchmark, development guide |

## Architecture at a Glance

```
main.cpp
  │
  ├── InMemoryEngine     ← unordered_map + shared_mutex (core data store)
  │
  ├── WalEngine          ← decorator over InMemoryEngine, writes WAL entries before mutations
  │                           replays WAL on startup to recover state
  │
  └── Server             ← epoll event loop, signalfd graceful shutdown
        │
        └── Connection   ← per-connection state (RESPReader + write buffer)
              │
              ├── RESPReader      ← incremental, buffer-safe RESP parser
              └── handle_command  ← dispatch to storage engine
```

The `IStorageEngine` interface (`storage/storage.h`) defines `get`, `set`, `del`, `exists`. `WalEngine` wraps `InMemoryEngine` using the decorator pattern — write operations are logged to `wal.log` before applying to memory; read operations pass straight through.

## Key Files

| File | Purpose |
|---|---|
| `src/main.cpp` | Application entrypoint: wires components together |
| `src/server/server.cpp` | TCP server with epoll, signalfd |
| `src/server/connection.cpp` | Per-connection read/write state machine |
| `src/protocol/resp.cpp` | RESP parser and serializer |
| `src/commands/handler.cpp` | Command dispatch (PING, SET, GET, DEL, EXISTS) |
| `src/storage/storage.h` | `IStorageEngine` interface |
| `src/storage/in_memory_storage.cpp` | `InMemoryEngine` — unordered_map + shared_mutex |
| `src/storage/wal_engine.cpp` | `WalEngine` — write-ahead log decorator |
| `tests/test_resp.cpp` | RESP parser unit tests (17 cases) |
| `tests/test_storage.cpp` | Storage engine unit tests (13 cases) |
| `benchmarks/bench_resp.cpp` | Benchmark skeleton (empty, future work) |

## Git History Highlights

- `6bb5eb1` — Initial commit: TCP server, RESP parser, in-memory storage, basic commands
- `446c429` — Clang-format pass across all source files
- `4b607aa` — Write-Ahead Log (WAL) engine implementation
- `c3e83c8` — Google Test integration with 36 unit tests
- `b7bf518` — Error handling and graceful shutdown (signalfd, WAL corruption skip)
- `6037674` — .gitignore cleanup, removed build artifacts from tracking
