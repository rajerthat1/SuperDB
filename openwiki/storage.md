# Storage Layer

The storage layer uses an **interface + decorator pattern** to layer durability on top of in-memory storage.

## Interface (`src/storage/storage.h`)

```cpp
class IStorageEngine {
public:
  virtual ~IStorageEngine() = default;
  virtual std::optional<std::string> get(const std::string &key) = 0;
  virtual void set(const std::string &key, const std::string &value) = 0;
  virtual bool del(const std::string &key) = 0;
  virtual bool exists(const std::string &key) = 0;
};
```

This interface is the contract used by `handle_command()` and `Connection::on_readable()`. Every storage implementation (current and future) must satisfy it.

## InMemoryEngine (`src/storage/in_memory_storage.h`, `src/storage/in_memory_storage.cpp`)

A straightforward `std::unordered_map<std::string, std::string>` protected by a `std::shared_mutex`:

| Operation | Lock | Complexity |
|---|---|---|
| `get` | shared_lock | O(1) average |
| `set` | unique_lock | O(1) average |
| `del` | unique_lock | O(1) average |
| `exists` | shared_lock | O(1) average |

The `shared_mutex` is **not currently needed** — the server is single-threaded — but it exists to support future multi-threaded phases (e.g., background compaction, flush operations, concurrent benchmark clients).

## WalEngine (`src/storage/wal_engine.h`, `src/storage/wal_engine.cpp`)

`WalEngine` wraps any `IStorageEngine` using the **decorator pattern**. It intercepts write operations to append entries to a write-ahead log before applying them to the inner engine.

### Write Path (Durability)

```
Client → WalEngine::set("key", "val")
  1. Append RESP-encoded command to wal.log (via append())
  2. fdatasync() to flush OS cache to disk
  3. Call inner_.set("key", "val") to update memory
```

The same pattern applies to `del()`. Read operations (`get`, `exists`) pass straight through to `inner_` with no disk I/O.

### WAL Format

The WAL stores RESP-encoded commands so the same parser (`RESPReader`) used for client protocol can be reused for recovery:

```
SET  → *3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n
DEL  → *2\r\n$3\r\nDEL\r\n$3\r\nkey\r\n
```

### Recovery (`replay()`)

On startup, `main()` calls `wal.replay()`:

1. Open `wal.log` read-only
2. Read entire file into a string buffer (up to 64 KiB chunks)
3. Feed buffer to `RESPReader`
4. For each parsed command: re-apply SET and DEL to `inner_`
5. Corrupted entries (partial writes, garbage) are skipped via `reader.skip_one()` — only valid entries are recovered

This means partial WAL corruption or a truncated last entry does not prevent recovery of all prior data.

### File Management

- WAL file path is configurable (default `"wal.log"`)
- Opened with `O_CREAT | O_RDWR | O_APPEND`, permissions `0644`
- `fd_` is closed in the destructor
- No WAL rotation or compaction yet — the log grows unbounded until the process restarts

## Current Limitations (Phases Ahead)

| Limitation | Planned Fix | Phase |
|---|---|---|
| Unbounded WAL growth | LSM-tree compaction discards old log entries | Phase 2 |
| O(n) replay on startup | MemTable + SSTable skip full replay | Phase 2 |
| Single-threaded writes | Background flush/compaction threads | Phase 2-3 |
| No CRC on WAL entries | SIMD CRC32 via `_mm_crc32_u64` | Phase 3 |

## Testing

13 unit tests in `tests/test_storage.cpp`:

- **InMemoryEngine** (7 tests): set/get, nonexistent get, overwrite, del, del nonexistent, exists lifecycle, empty key, empty value
- **WalEngine** (6 tests via `WalTest` fixture): recovery after crash, no data on first run, delete logged, overwrite in WAL, partial last entry ignored, corrupt middle entry skipped

### When Changing Storage

- Run `test_storage` to verify correctness
- If adding a new storage backend (e.g., LSM MemTable), implement `IStorageEngine` and test independently
- If changing the WAL format (e.g., adding CRC), update both `append()` and `replay()`, and ensure corruption tests still pass
- The temp WAL path for tests is `/tmp/test_wal.log` — be aware of cross-contamination if tests crash
