#include "in_memory_storage.h"

std::optional<std::string> InMemoryEngine::get(const std::string& key) {
    std::shared_lock lock(mtx_);
    auto it = map_.find(key);
    if (it == map_.end()) return std::nullopt;
    return it->second;
}

void InMemoryEngine::set(const std::string& key, const std::string& value) {
    std::unique_lock lock(mtx_);
    map_[key] = value;
}

bool InMemoryEngine::del(const std::string& key) {
    std::unique_lock lock(mtx_);
    return map_.erase(key) > 0;
}

bool InMemoryEngine::exists(const std::string& key) {
    std::shared_lock lock(mtx_);
    return map_.count(key) > 0;
}
