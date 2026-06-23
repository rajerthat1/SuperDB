#include "server/server.h"
#include "storage/in_memory_storage.h"
#include "storage/wal_engine.h"
#include <iostream>

int main() {
  InMemoryEngine inner;
  WalEngine wal(inner, "wal.log");
  wal.replay();
  Server server(6379, wal);
  std::cout << "Listening on port 6379..." << std::endl;
  server.run();
  std::cout << "Shutting down..." << std::endl;
  return 0;
}
