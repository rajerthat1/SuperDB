# Protocol & Command Handling

## RESP Protocol

The project implements the Redis Serialization Protocol (RESP) — a text-based wire protocol used by Redis clients and servers. RESP supports five data types that map to the `Reply` variant:

| RESP Type | Wire Format | C++ Representation |
|---|---|---|
| Simple String | `+OK\r\n` | `reply::Simple` |
| Bulk String | `$5\r\nhello\r\n` | `reply::Bulk` |
| Null Bulk String | `$-1\r\n` | `reply::Nil` |
| Integer | `:42\r\n` | `reply::Integer` |
| Error | `-ERR msg\r\n` | `reply::Error` |

Commands are sent as RESP Arrays: `*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n`

## Parser (`src/protocol/resp.h`, `src/protocol/resp.cpp`)

`RESPReader` is an **incremental, buffer-safe** parser:

```cpp
class RESPReader {
public:
  void feed(const char *data, size_t len);     // append data to internal buffer
  std::optional<Command> try_parse();           // attempt to parse one command
  static std::string serialize(const Reply &r); // serialize a Reply to wire format
  void reset();                                 // clear internal buffer
  bool has_data() const;                        // any data in buffer?
  bool starts_with_array() const;               // does buffer start with '*'?
  size_t buffer_size() const;                   // current buffer size
  void skip_one();                              // discard one byte (WAL corruption recovery)
};
```

### Parsing Pipeline

1. `feed()` appends raw bytes to an internal `std::string` buffer
2. `try_parse()` attempts to parse a RESP array from the buffer:
   - Reads `*` + integer count + `\r\n`
   - For each element, reads `$` + length + `\r\n` + data + `\r\n` (bulk string)
   - If any step fails (insufficient data, wrong prefix), returns `std::nullopt` — the caller should wait for more data
3. On success, returns a `Command{name, args}` and advances the internal position. The caller can call `try_parse()` again for pipelined commands
4. On parse failure (garbage data), the caller must `skip_one()` to advance past the problematic byte

### Serialization

`RESPReader::serialize()` converts a `Reply` variant to its RESP wire format. Used by `handle_command()` → `Connection::on_readable()` to build the write buffer.

### WAL Replay Integration

`WalEngine::replay()` reads the entire WAL file into a buffer, feeds it to a `RESPReader`, and re-applies each parsed SET/DEL command to the inner storage engine. Corrupted or partial entries at the end of the WAL are skipped via `reader.skip_one()` — the WAL is append-only so only the last entry can be truncated.

## Supported Commands (`src/commands/handler.cpp`)

`handle_command()` receives a parsed `Command` and a reference to `IStorageEngine`, then dispatches:

| Command | Args | Behavior | Reply |
|---|---|---|---|
| `PING` | 0 | Return `PONG` | `+PONG\r\n` |
| `PING` | 1+ | Echo the first argument | `$len\r\narg\r\n` |
| `SET` | key, value, ... | Store key → value | `+OK\r\n` |
| `GET` | key | Retrieve value (or nil) | `$len\r\nvalue\r\n` or `$-1\r\n` |
| `DEL` | key1 key2 ... | Delete keys, return count | `:count\r\n` |
| `EXISTS` | key1 key2 ... | Count existing keys | `:count\r\n` |
| Unknown | — | Return error | `-ERR unknown command 'X'\r\n` |

Command names are case-insensitive (converted to uppercase via `to_upper`).

## Testing

17 unit tests in `tests/test_resp.cpp` cover:

- Parsing each supported command (PING, SET, GET, DEL, EXISTS)
- Partial data returns nullopt (incremental parsing)
- Empty feed
- Pipelined commands (multiple commands in one buffer)
- Null bulk strings (`$-1`)
- Serialization of all reply types (Simple, Bulk, Null, Integer, Error, Empty Bulk)
- `RESPReader` state introspection (has_data, starts_with_array, buffer_size, reset, skip_one)

### When Changing the Protocol

- Run `test_resp` to verify parsing and serialization
- If adding a new command, add it to both `commands/handler.cpp` and the test file
- If changing the WAL format, update `WalEngine::replay()` and `WalEngine::set()`/`del()` which write RESP-encoded commands to the log
- The benchmark file `benchmarks/bench_resp.cpp` is currently empty — a future home for throughput testing
