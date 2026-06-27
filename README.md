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

### Phase 5 — Advanced Networking
- [ ] Unix domain sockets (`AF_UNIX`) — local client IPC, bypass TCP stack
- [ ] TCP keepalive (`SO_KEEPALIVE`, `TCP_KEEPIDLE`) — dead peer detection
- [ ] Proper connection teardown — `SO_LINGER`, half-close via `shutdown()`
- [ ] Protocol-agnostic address resolution (`getaddrinfo`, IPv4 + IPv6)
- [ ] Non-blocking connect — foundation for async replication client
- [ ] UDP multicast service discovery — peers find each other on LAN
- [ ] Out-of-band data handling

**Topics:** Unix domain protocols, TCP tuning, graceful shutdown, address
  resolution, concurrent connect patterns, multicast, signal-driven I/O

---

## How to build & run

```bash
cmake -B build && cmake --build build
./build/kv-server
redis-cli PING
```
