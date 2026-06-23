#include "storage/wal_engine.h"
#include "protocol/resp.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

// open/create the wal.log file in append mode. O_RDWR read/write , O_CREAT
// create if missing, O_APPEND to write to the end of the file
WalEngine::WalEngine(IStorageEngine &inner, const std::string &path)
    : inner_(inner), path_(path) {
  fd_ = open(path.c_str(), O_CREAT | O_RDWR | O_APPEND, 0644);
  if (fd_ < 0)
    throw std::runtime_error("Failed to open WAL file: " + path);
}

// closes the fd. file cleaned up when WalEngine out of scope
WalEngine::~WalEngine() {
  if (fd_ >= 0)
    close(fd_);
}

// writes the data to the log file. the while loop handles the partial writes
// (e.g on_writable), fdatasync() forces the data fro OS cache to disk. make
// sure log entry is saved
void WalEngine::append(const std::string &data) {
  size_t off = 0;
  while (off < data.size()) {
    ssize_t n = write(fd_, data.data() + off, data.size() - off);
    if (n <= 0)
      return;
    off += n;
  }
  fdatasync(fd_);
}

std::optional<std::string> WalEngine::get(const std::string &key) {
  return inner_.get(key);
}

void WalEngine::set(const std::string &key, const std::string &value) {
  std::string entry = std::string("*3\r\n$3\r\nSET\r\n") + "$" +
                      std::to_string(key.size()) + "\r\n" + key + "\r\n" + "$" +
                      std::to_string(value.size()) + "\r\n" + value + "\r\n";
  append(entry);          // write to log
  inner_.set(key, value); // add to memory
}

// same as set, append to log, then apply to inner_
bool WalEngine::del(const std::string &key) {
  std::string entry = std::string("*2\r\n$3\r\nDEL\r\n") + "$" +
                      std::to_string(key.size()) + "\r\n" + key + "\r\n";
  append(entry);
  return inner_.del(key);
}

// get() and exist() reads pas stragith through inner_ , no disk I/O needed
bool WalEngine::exists(const std::string &key) { return inner_.exists(key); }

// Rebuild the hashmap on startup
// Read the entire log file into buffer (string)
// feed it to a RESPReader
// parse and re-apply each command to inner_
void WalEngine::replay() {
  int fd = open(path_.c_str(), O_RDONLY);
  if (fd < 0)
    return;

  std::string buf;
  char tmp[65536];
  ssize_t n;
  while ((n = read(fd, tmp, sizeof(tmp))) > 0)
    buf.append(tmp, n);
  close(fd);

  if (buf.empty())
    return;

  RESPReader reader;
  reader.feed(buf.data(), buf.size());
  while (reader.has_data()) {
    auto cmd = reader.try_parse();
    if (cmd) {
      if (cmd->name == "SET" && cmd->args.size() >= 2)
        inner_.set(cmd->args[0], cmd->args[1]);
      else if (cmd->name == "DEL" && cmd->args.size() >= 1)
        for (const auto &key : cmd->args)
          inner_.del(key);
    } else {
      reader.skip_one();
    }
  }
}
