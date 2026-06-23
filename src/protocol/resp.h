#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

struct Command {
  std::string name;
  std::vector<std::string> args;
};

namespace reply {
struct Simple {
  std::string val;
};
struct Bulk {
  std::string val;
};
struct Nil {};
struct Integer {
  int64_t val;
};
struct Error {
  std::string msg;
};
} // namespace reply

using Reply = std::variant<reply::Simple, reply::Bulk, reply::Nil,
                           reply::Integer, reply::Error>;

class RESPReader {
public:
  void feed(const char *data, size_t len);
  std::optional<Command> try_parse();
  static std::string serialize(const Reply &reply);

  void reset();
  bool has_data() const;
  bool starts_with_array() const;
  size_t buffer_size() const;
  void skip_one();

private:
  std::string buf_;
};
