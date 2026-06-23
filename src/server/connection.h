#pragma once
#include "protocol/resp.h"
#include "storage/storage.h"
#include <string>

struct Connection {
  RESPReader parser;
  std::string write_buf;
  bool close_after_flush = false;

  void on_readable(const char *data, size_t len, IStorageEngine &storage);
  void on_writable(int fd);
  bool should_close() const;

  static constexpr size_t kMaxReadBuf = 256 * 1024;
};
