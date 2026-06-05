#pragma once
#include "protocol/resp.h"
#include "storage/storage.h"
#include <string>

struct Connection {
    RESPReader parser;
    std::string write_buf;

    void on_readable(const char* data, size_t len, IStorageEngine& storage);
    void on_writable(int fd);
};
