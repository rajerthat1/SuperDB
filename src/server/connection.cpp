#include "server/connection.h"
#include "commands/handler.h"
#include <cerrno>
#include <unistd.h>

bool Connection::should_close() const {
  return close_after_flush && write_buf.empty();
}

void Connection::on_readable(const char *data, size_t len,
                             IStorageEngine &storage) {
  if (close_after_flush)
    return;

  parser.feed(data, len);

  while (auto cmd = parser.try_parse())
    write_buf += RESPReader::serialize(handle_command(*cmd, storage));

  if (parser.has_data() && !parser.starts_with_array()) {
    write_buf += RESPReader::serialize(
        reply::Error{"protocol error: expected RESP array"});
    close_after_flush = true;
    parser.reset();
    return;
  }

  if (parser.buffer_size() > kMaxReadBuf) {
    write_buf += RESPReader::serialize(
        reply::Error{"protocol error: request too large"});
    close_after_flush = true;
    parser.reset();
    return;
  }
}

void Connection::on_writable(int fd) {
  if (write_buf.empty())
    return;
  ssize_t n = write(fd, write_buf.data(), write_buf.size());
  if (n > 0) {
    write_buf.erase(0, n);
  } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    close_after_flush = true;
    write_buf.clear();
  }
}
