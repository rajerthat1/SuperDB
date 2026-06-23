# key_val — Redis-like key-value store

Building a Redis-compatible key-value store from scratch in C++ to explore
systems programming, storage engines, and distributed consensus.

## Roadmap

### Phase 0 — Foundation (done)
- [x] TCP server with epoll event loop (single-threaded, like Redis)
- [x] RESP protocol parser — incremental, buffer-safe
- [x] In-memory `unordered_map` + `shared_mutex` (RwLock)
- [x] Commands: PING, SET, GET, DEL, EXISTS
- [x] CMake build system

**Concepts:** socket programming, epoll, non-blocking I/O, RESP wire protocol,
  interface-based design (IStorageEngine)

---

### Phase 1 — Core Reliability
- [x] Write-Ahead Log (WAL) — fsync every write before updating memory
- [x] Unit testing framework (Google Test, 36 tests)
- [x] Error handling — buffer limits, garbage detection, WAL corruption skip, socket error handling
- [x] Graceful shutdown — signalfd, drain connections, close fds cleanly

**Topics:** crash recovery, serialization, durability, testing methodology

---

### Phase 2 — Storage Engine (LSM Tree)
- [ ] MemTable (sorted in-memory structure, replace `unordered_map`)
- [ ] SSTable format — immutable, sorted, on-disk KV blocks + index
- [ ] Flush MemTable → SSTable on threshold
- [ ] Compaction — merge overlapping SSTables, discard tombstones
- [ ] Thread-safe LRU cache (hot keys bypass LSM)

**Topics:** LSM tree, sorted string tables, compaction strategies, cache
  invalidation, semaphores for throttling writes

---

### Phase 3 — Advanced Systems
- [ ] Lock-free skip list (replace mutex-based MemTable)
- [ ] Benchmark suite / `/proc` metrics monitoring
- [ ] Memory paging awareness (huge pages, mmap for SSTables)
- [ ] CPU scheduling / coalescing / io_uring (replace epoll)

**Topics:** lock-free data structures, memory ordering, hazard pointers,
  manual atomics, kernel bypass, performance profiling

---

### Phase 4 — Distributed
- [ ] Raft consensus (leader election, log replication, safety)
- [ ] Replication + quorum reads/writes
- [ ] Sharding via consistent hashing
- [ ] Gossip protocol for cluster membership
- [ ] Distributed transactions

**Topics:** consensus algorithms, CAP theorem, quorum, vector clocks,
  failure detection, gossip

---

## How to build & run

```bash
cmake -B build && cmake --build build
./build/kv-server
redis-cli PING
```
deal with scaling issues
