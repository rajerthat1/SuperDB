# Operations & Testing

## Building

### Prerequisites

- CMake 3.20+
- C++20 compiler (GCC 10+, Clang 10+, or equivalent)
- Linux (uses `epoll`, `signalfd`, Linux-specific headers)
- pthreads library

### Build Commands

```bash
# Configure and build (from repository root)
cmake -B build && cmake --build build

# The binary is at: ./build/kv-server
```

The CMake configuration (`CMakeLists.txt`) uses:
- `-O2 -Wall -Wextra -g` for all builds
- Tests are enabled by default (`BUILD_TESTS=ON`)
- Google Test is fetched automatically via `FetchContent` (version 1.14.0)

### Build Targets

| Target | Description | Source Files |
|---|---|---|
| `kv-server` | Main server binary | `src/main.cpp`, `src/server/*.cpp`, `src/protocol/resp.cpp`, `src/storage/*.cpp`, `src/commands/handler.cpp` |
| `test_resp` | RESP parser/serializer unit tests | `tests/test_resp.cpp`, `src/protocol/resp.cpp` |
| `test_storage` | Storage engine unit tests | `tests/test_storage.cpp`, `src/storage/*.cpp`, `src/protocol/resp.cpp` |

## Running

```bash
./build/kv-server
# Output: "Listening on port 6379..."
```

The server listens on `0.0.0.0:6379`. Press Ctrl+C to trigger graceful shutdown (via signalfd → SIGINT).

### Testing with redis-cli

```bash
# Install redis-cli (Ubuntu/Debian)
sudo apt-get install redis-tools

# Or (macOS)
brew install redis

# Then interact:
redis-cli PING                          # +PONG
redis-cli SET mykey "hello world"       # +OK
redis-cli GET mykey                     # $11\r\nhello world\r\n
redis-cli EXISTS mykey                  # :1
redis-cli DEL mykey                     # :1
redis-cli GET mykey                     # $-1\r\n (nil)

# Pipelining: send multiple commands at once
redis-cli --pipe < commands.txt
```

## Testing

```bash
# Run all tests via CTest
cd build && ctest --output-on-failure

# Or run individual test binaries directly
./build/tests/test_resp
./build/tests/test_storage
```

### Test Suites

| Suite | Tests | What It Covers |
|---|---|---|
| `RespParser` | 17 | RESP parsing, serialization, pipelining, edge cases |
| `InMemoryEngine` | 7 | Basic CRUD, overwrite, empty key/value, exists lifecycle |
| `WalTest` | 6 | WAL recovery, crash simulation, corruption handling |

**36 total tests** across all suites.

### Adding Tests

1. Add new test cases to the existing files (`tests/test_resp.cpp`, `tests/test_storage.cpp`)
2. Or create a new test file in `tests/` and add an `add_executable` + `gtest_discover_tests` entry in `tests/CMakeLists.txt`
3. Rebuild: `cmake --build build -j$(nproc)`

### Interpreting Failures

- **RespParser failures** — typically indicate a change in the parser or serializer logic
- **InMemoryEngine failures** — indicate a bug in the in-memory map or locking
- **WalTest failures** — indicate issues in WAL serialization format, `append()`, or `replay()`. The corruption tests (`PartialLastEntryIgnored`, `CorruptMiddleEntrySkipped`) are particularly sensitive to changes in the parser or WAL format

## Benchmarking

The file `benchmarks/bench_resp.cpp` is currently **empty** — it was created in the initial commit as a placeholder for future RESP parser throughput benchmarks.

To add benchmarks:

1. Implement benchmark logic in `benchmarks/bench_resp.cpp`
2. Add a `add_executable` entry in `CMakeLists.txt` for the benchmark target
3. Consider using Google Benchmark or a simple custom harness

Future benchmark plans (Phase 3 roadmap) include:
- RESP parsing throughput (especially SIMD-accelerated variants)
- In-memory storage get/set latency
- WAL write throughput
- Connection handling under load

## Development Notes

### Code Style

All source files have been formatted with `clang-format` (commit `446c429`). New code should follow the existing style — 2-space indentation, consistent brace placement.

### Repository Structure

```
key_val/
├── src/
│   ├── main.cpp              # Entrypoint
│   ├── commands/
│   │   ├── handler.h
│   │   └── handler.cpp
│   ├── protocol/
│   │   ├── resp.h
│   │   └── resp.cpp
│   ├── server/
│   │   ├── server.h
│   │   ├── server.cpp
│   │   ├── connection.h
│   │   └── connection.cpp
│   └── storage/
│       ├── storage.h          # IStorageEngine interface
│       ├── in_memory_storage.h
│       ├── in_memory_storage.cpp
│       ├── wal_engine.h
│       └── wal_engine.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── test_resp.cpp
│   └── test_storage.cpp
├── benchmarks/
│   └── bench_resp.cpp         # Skeleton (empty)
├── CMakeLists.txt
├── README.md
└── wal.log                    # WAL file (created at runtime)
```

### Known Technical Debt

1. **`close_after_flush` connection close not implemented** — connections with protocol errors are never actively closed; they linger until the client disconnects. See `Connection::should_close()` which is never called from the event loop.
2. **WAL file grows unbounded** — no rotation or compaction. The entire log is replayed on startup.
3. **No connection timeout** — idle connections consume memory indefinitely.
4. **Write buffer uses `std::string::erase(0, n)`** — O(n) memmove on every partial write. A ring buffer would be O(1).
5. **No client authentication** — any network client can connect and issue commands.
