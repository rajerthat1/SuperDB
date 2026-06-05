#pragma once
#include <cstdint>
#include "storage/storage.h"

class Server {
public:
    Server(uint16_t port, IStorageEngine& storage);
    void run();
private:
    uint16_t port_;
    IStorageEngine& storage_;
};
