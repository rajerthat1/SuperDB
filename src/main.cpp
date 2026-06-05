#include "server/server.h"
#include "storage/in_memory_storage.h"
#include <iostream>

int main() {
    InMemoryEngine storage;
    Server server(6379, storage);
    std::cout << "Listening on port 6379..." << std::endl;
    server.run();
    return 0;
}
