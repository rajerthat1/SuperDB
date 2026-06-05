#include "server/connection.h"
#include "commands/handler.h"
#include <unistd.h>

void Connection::on_readable(const char* data, size_t len, IStorageEngine& storage) {
    parser.feed(data, len);
    while (auto cmd = parser.try_parse())
        write_buf += RESPReader::serialize(handle_command(*cmd, storage));
}

void Connection::on_writable(int fd) {
    if (write_buf.empty()) return;
    ssize_t n = write(fd, write_buf.data(), write_buf.size());
    if (n > 0) write_buf.erase(0, n);
}
