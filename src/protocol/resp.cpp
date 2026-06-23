#include "protocol/resp.h"
#include <cctype>

void RESPReader::feed(const char *data, size_t len) { buf_.append(data, len); }

static bool try_consume(const std::string &buf, size_t &pos,
                        const std::string &expected) {
  if (pos + expected.size() > buf.size())
    return false;
  if (buf.compare(pos, expected.size(), expected) != 0)
    return false;
  pos += expected.size();
  return true;
}

static bool try_parse_int(const std::string &buf, size_t &pos, int64_t &out) {
  size_t start = pos;
  if (pos >= buf.size() || !std::isdigit(buf[pos]))
    return false;
  out = 0;
  while (pos < buf.size() && std::isdigit(buf[pos])) {
    out = out * 10 + (buf[pos] - '0');
    pos++;
  }
  if (!try_consume(buf, pos, "\r\n")) {
    pos = start;
    return false;
  }
  return true;
}

static bool try_parse_bulk_str(const std::string &buf, size_t &pos,
                               std::string &out) {
  size_t start = pos;
  if (pos >= buf.size() || buf[pos] != '$')
    return false;
  pos++;

  if (pos < buf.size() && buf[pos] == '-') {
    pos++;
    if (!try_consume(buf, pos, "1\r\n")) {
      pos = start;
      return false;
    }
    out = "";
    return true;
  }

  int64_t len;
  if (!try_parse_int(buf, pos, len)) {
    pos = start;
    return false;
  }

  if (pos + len + 2 > buf.size()) {
    pos = start;
    return false;
  }
  out = buf.substr(pos, len);
  pos += len;
  if (!try_consume(buf, pos, "\r\n")) {
    pos = start;
    return false;
  }
  return true;
}

static bool try_parse_array(const std::string &buf, size_t &pos,
                            std::vector<std::string> &out) {
  size_t start = pos;
  if (pos >= buf.size() || buf[pos] != '*')
    return false;
  pos++;

  int64_t count;
  if (!try_parse_int(buf, pos, count)) {
    pos = start;
    return false;
  }

  out.clear();
  for (int64_t i = 0; i < count; i++) {
    std::string elem;
    if (!try_parse_bulk_str(buf, pos, elem)) {
      pos = start;
      return false;
    }
    out.push_back(std::move(elem));
  }
  return true;
}

std::optional<Command> RESPReader::try_parse() {
  if (buf_.empty())
    return std::nullopt;
  size_t pos = 0;
  std::vector<std::string> parts;
  if (!try_parse_array(buf_, pos, parts))
    return std::nullopt;
  if (parts.empty())
    return std::nullopt;

  buf_.erase(0, pos);

  Command cmd;
  cmd.name = std::move(parts[0]);
  for (size_t i = 1; i < parts.size(); i++)
    cmd.args.push_back(std::move(parts[i]));
  return cmd;
}

void RESPReader::reset() { buf_.clear(); }

bool RESPReader::has_data() const { return !buf_.empty(); }

bool RESPReader::starts_with_array() const {
  return !buf_.empty() && buf_[0] == '*';
}

size_t RESPReader::buffer_size() const { return buf_.size(); }

void RESPReader::skip_one() {
  if (!buf_.empty())
    buf_.erase(0, 1);
}

std::string RESPReader::serialize(const Reply &reply) {
  return std::visit(
      [](const auto &r) -> std::string {
        using T = std::decay_t<decltype(r)>;
        if constexpr (std::is_same_v<T, reply::Simple>) {
          return "+" + r.val + "\r\n";
        } else if constexpr (std::is_same_v<T, reply::Bulk>) {
          return "$" + std::to_string(r.val.size()) + "\r\n" + r.val + "\r\n";
        } else if constexpr (std::is_same_v<T, reply::Nil>) {
          return "$-1\r\n";
        } else if constexpr (std::is_same_v<T, reply::Integer>) {
          return ":" + std::to_string(r.val) + "\r\n";
        } else if constexpr (std::is_same_v<T, reply::Error>) {
          return "-ERR " + r.msg + "\r\n";
        }
      },
      reply);
}
