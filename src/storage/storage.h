#pragma once
#include <string>
#include <optional>

class IStorageEngine {
public:
    virtual ~IStorageEngine() = default;
    virtual std::optional<std::string> get(const std::string& key) = 0;
    virtual void set(const std::string& key, const std::string& value) = 0;
    virtual bool del(const std::string& key) = 0;
    virtual bool exists(const std::string& key) = 0;
};
