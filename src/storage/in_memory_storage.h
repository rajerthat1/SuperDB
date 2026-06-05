#pragma once
#include "storage/storage.h"
#include <unordered_map>
#include <shared_mutex>
#include <mutex>

class InMemoryEngine : public IStorageEngine {
public:
    std::optional<std::string> get(const std::string& key) override;
    void set(const std::string& key, const std::string& value) override;
    bool del(const std::string& key) override;
    bool exists(const std::string& key) override;
private:
    std::unordered_map<std::string, std::string> map_;
    mutable std::shared_mutex mtx_;
};
