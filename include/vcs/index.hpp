#pragma once

#include "filesystem.hpp"
#include "hash.hpp"

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>

namespace vcs {

struct IndexEntry {
    std::string path;
    std::string hash;
    uint64_t mtime{0};
    uint64_t size{0};
    bool staged{false};

    bool operator==(const IndexEntry& other) const {
        return path == other.path && hash == other.hash;
    }
};

class Index {
public:
    explicit Index(const std::string& vcsDir);

    bool load();
    bool save() const;

    void stage(const std::string& path, const std::string& hash,
               uint64_t mtime, uint64_t size);
    void unstage(const std::string& path);
    bool isStaged(const std::string& path) const;

    std::vector<IndexEntry> getStagedFiles() const;
    std::vector<IndexEntry> getAllFiles() const;
    std::string getHash(const std::string& path) const;

    bool hasFile(const std::string& path) const;
    void clear();

    std::string serialize() const;
    bool deserialize(const std::string& data);

private:
    std::string indexFile_;
    std::map<std::string, IndexEntry> entries_;
};

inline Index::Index(const std::string& vcsDir)
    : indexFile_(vcsDir + "/index") {}

inline bool Index::load() {
    if (!FileSystem::exists(indexFile_)) return true;
    auto data = FileSystem::readTextFile(indexFile_);
    return deserialize(data);
}

inline bool Index::save() const {
    return FileSystem::writeTextFile(indexFile_, serialize());
}

inline void Index::stage(const std::string& path, const std::string& hash,
                          uint64_t mtime, uint64_t size) {
    IndexEntry entry;
    entry.path = path;
    entry.hash = hash;
    entry.mtime = mtime;
    entry.size = size;
    entry.staged = true;
    entries_[path] = entry;
}

inline void Index::unstage(const std::string& path) {
    entries_.erase(path);
}

inline bool Index::isStaged(const std::string& path) const {
    auto it = entries_.find(path);
    return it != entries_.end() && it->second.staged;
}

inline std::vector<IndexEntry> Index::getStagedFiles() const {
    std::vector<IndexEntry> result;
    for (auto& [path, entry] : entries_)
        if (entry.staged) result.push_back(entry);
    return result;
}

inline std::vector<IndexEntry> Index::getAllFiles() const {
    std::vector<IndexEntry> result;
    for (auto& [path, entry] : entries_)
        result.push_back(entry);
    return result;
}

inline std::string Index::getHash(const std::string& path) const {
    auto it = entries_.find(path);
    return (it != entries_.end()) ? it->second.hash : "";
}

inline bool Index::hasFile(const std::string& path) const {
    return entries_.find(path) != entries_.end();
}

inline void Index::clear() { entries_.clear(); }

inline std::string Index::serialize() const {
    std::ostringstream oss;
    for (auto& [path, entry] : entries_) {
        oss << entry.hash << " " << entry.mtime << " "
            << entry.size << " " << (entry.staged ? "1" : "0")
            << " " << path << "\n";
    }
    return oss.str();
}

inline bool Index::deserialize(const std::string& data) {
    entries_.clear();
    std::istringstream iss(data);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        std::istringstream lineStream(line);
        IndexEntry entry;
        std::string stagedStr;
        if (!(lineStream >> entry.hash >> entry.mtime >> entry.size >> stagedStr))
            continue;
        entry.staged = (stagedStr == "1");
        std::getline(lineStream >> std::ws, entry.path);
        if (!entry.path.empty() && entry.path[0] == ' ')
            entry.path = entry.path.substr(1);
        entries_[entry.path] = entry;
    }
    return true;
}

} // namespace vcs
