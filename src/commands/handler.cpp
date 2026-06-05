#include "commands/handler.h"
#include <algorithm>
#include <cctype>

static std::string to_upper(std::string s) {
    for (auto& c : s) c = std::toupper(c);
    return s;
}

Reply handle_command(const Command& cmd, IStorageEngine& storage) {
    std::string name = to_upper(cmd.name);

    if (name == "PING") {
        if (cmd.args.empty()) return reply::Simple{"PONG"};
        return reply::Bulk{cmd.args[0]};
    }

    if (name == "SET" && cmd.args.size() >= 2) {
        storage.set(cmd.args[0], cmd.args[1]);
        return reply::Simple{"OK"};
    }

    if (name == "GET" && cmd.args.size() == 1) {
        auto val = storage.get(cmd.args[0]);
        if (val) return reply::Bulk{*val};
        return reply::Nil{};
    }

    if (name == "DEL" && cmd.args.size() >= 1) {
        int64_t count = 0;
        for (const auto& key : cmd.args)
            if (storage.del(key)) count++;
        return reply::Integer{count};
    }

    if (name == "EXISTS" && cmd.args.size() >= 1) {
        int64_t count = 0;
        for (const auto& key : cmd.args)
            if (storage.exists(key)) count++;
        return reply::Integer{count};
    }

    return reply::Error{"unknown command '" + cmd.name + "'"};
}
