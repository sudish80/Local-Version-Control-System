#pragma once

#include "compression.hpp"
#include "filesystem.hpp"
#include "hash.hpp"

#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <ctime>
#include <memory>

namespace vcs {

enum class ObjectType { Unknown, Blob, Tree, Commit };

struct Object {
    ObjectType type{ObjectType::Unknown};
    std::vector<uint8_t> content;
    std::string hash;

    std::string computeHash() const {
        auto raw = serialize();
        return SHA1::hash(std::string(raw.begin(), raw.end()));
    }

    std::vector<uint8_t> serialize() const {
        std::string prefix;
        switch (type) {
            case ObjectType::Blob:   prefix = "blob ";   break;
            case ObjectType::Tree:   prefix = "tree ";   break;
            case ObjectType::Commit: prefix = "commit "; break;
            default: throw std::runtime_error("Unknown object type");
        }
        prefix += std::to_string(content.size());
        prefix += '\0';
        std::vector<uint8_t> result(prefix.begin(), prefix.end());
        result.insert(result.end(), content.begin(), content.end());
        return result;
    }

    static Object deserialize(const std::vector<uint8_t>& raw) {
        Object obj;
        auto nullPos = raw.begin();
        while (nullPos != raw.end() && *nullPos != 0) ++nullPos;
        if (nullPos == raw.end())
            throw std::runtime_error("Invalid object: no null terminator");

        std::string header(raw.begin(), nullPos);
        auto spacePos = header.find(' ');
        if (spacePos == std::string::npos)
            throw std::runtime_error("Invalid object header");

        auto typeStr = header.substr(0, spacePos);
        if (typeStr == "blob")      obj.type = ObjectType::Blob;
        else if (typeStr == "tree") obj.type = ObjectType::Tree;
        else if (typeStr == "commit") obj.type = ObjectType::Commit;
        else throw std::runtime_error("Unknown object type: " + typeStr);

        obj.content.assign(nullPos + 1, raw.end());
        return obj;
    }
};

struct TreeEntry {
    std::string mode;
    std::string name;
    std::string hash;
};

class ObjectDatabase {
public:
    explicit ObjectDatabase(const std::string& vcsDir);

    std::string store(const Object& obj);
    Object read(const std::string& hash) const;
    bool exists(const std::string& hash) const;

    std::string storeBlob(const std::vector<uint8_t>& content);
    std::string storeBlobFromFile(const std::string& filePath);
    std::string storeTree(const std::vector<TreeEntry>& entries);
    std::string storeCommit(const std::string& treeHash,
                            const std::string& parentHash,
                            const std::string& author,
                            const std::string& message);

    std::vector<TreeEntry> parseTree(const std::string& hash) const;
    std::string hashToPath(const std::string& hash) const;
    std::string resolveShortHash(const std::string& prefix) const;

private:
    std::string vcsDir_;
    std::string objectsDir_;
};

inline ObjectDatabase::ObjectDatabase(const std::string& vcsDir)
    : vcsDir_(vcsDir), objectsDir_(vcsDir + "/objects") {}

inline std::string ObjectDatabase::hashToPath(const std::string& hash) const {
    if (hash.length() < 2) return "";
    return objectsDir_ + "/" + hash.substr(0, 2) + "/" + hash.substr(2);
}

inline std::string ObjectDatabase::store(const Object& obj) {
    auto hash = obj.computeHash();
    auto path = hashToPath(hash);
    if (FileSystem::exists(path)) return hash;

    auto raw = obj.serialize();
    auto compressed = Compression::compress(
        std::vector<uint8_t>(raw.begin(), raw.end()));

    auto dir = objectsDir_ + "/" + hash.substr(0, 2);
    FileSystem::createDirectories(dir);
    FileSystem::writeFile(path, compressed);
    return hash;
}

inline Object ObjectDatabase::read(const std::string& hash) const {
    auto path = hashToPath(hash);
    if (!FileSystem::exists(path)) {
        throw std::runtime_error("Object not found: " + hash);
    }
    auto compressed = FileSystem::readFile(path);
    auto raw = Compression::decompress(compressed);
    auto obj = Object::deserialize(raw);
    obj.hash = hash;
    return obj;
}

inline bool ObjectDatabase::exists(const std::string& hash) const {
    return FileSystem::exists(hashToPath(hash));
}

inline std::string ObjectDatabase::storeBlob(const std::vector<uint8_t>& content) {
    Object obj; obj.type = ObjectType::Blob; obj.content = content;
    return store(obj);
}

inline std::string ObjectDatabase::storeBlobFromFile(const std::string& filePath) {
    return storeBlob(FileSystem::readFile(filePath));
}

inline std::string ObjectDatabase::storeTree(const std::vector<TreeEntry>& entries) {
    std::vector<uint8_t> content;
    for (auto& entry : entries) {
        auto modeStr = entry.mode + " " + entry.name;
        content.insert(content.end(), modeStr.begin(), modeStr.end());
        content.push_back('\0');
        auto hashBytes = entry.hash;
        content.insert(content.end(), hashBytes.begin(), hashBytes.end());
    }
    Object obj; obj.type = ObjectType::Tree; obj.content = std::move(content);
    return store(obj);
}

inline std::string ObjectDatabase::storeCommit(const std::string& treeHash,
                                                const std::string& parentHash,
                                                const std::string& author,
                                                const std::string& message) {
    std::ostringstream oss;
    oss << "tree " << treeHash << "\n";
    if (!parentHash.empty()) oss << "parent " << parentHash << "\n";
    oss << "author " << author << " " << std::time(nullptr) << "\n";
    oss << "\n" << message << "\n";
    Object obj; obj.type = ObjectType::Commit;
    auto content = oss.str();
    obj.content.assign(content.begin(), content.end());
    return store(obj);
}

inline std::string ObjectDatabase::resolveShortHash(const std::string& prefix) const {
    if (prefix.size() >= 40) return prefix;
    if (prefix.size() < 2) return "";
    auto dir = objectsDir_ + "/" + prefix.substr(0, 2);
    if (!FileSystem::isDirectory(dir)) return "";
    auto files = FileSystem::listDirectory(dir);
    std::vector<std::string> matches;
    for (auto& f : files) {
        auto full = prefix.substr(0, 2) + f;
        if (full.substr(0, prefix.size()) == prefix) matches.push_back(full);
    }
    return (matches.size() == 1) ? matches[0] : "";
}

inline std::vector<TreeEntry> ObjectDatabase::parseTree(const std::string& hash) const {
    auto obj = read(hash);
    if (obj.type != ObjectType::Tree)
        throw std::runtime_error("Not a tree object: " + hash);

    std::vector<TreeEntry> entries;
    auto& data = obj.content;
    size_t pos = 0;

    while (pos < data.size()) {
        TreeEntry entry;
        size_t spacePos = data.size();
        for (size_t i = pos; i < data.size(); ++i)
            if (data[i] == ' ') { spacePos = i; break; }
        if (spacePos >= data.size()) break;
        entry.mode.assign(data.begin() + pos, data.begin() + spacePos);
        pos = spacePos + 1;

        size_t nullPos = data.size();
        for (size_t i = pos; i < data.size(); ++i)
            if (data[i] == 0) { nullPos = i; break; }
        if (nullPos >= data.size()) break;
        entry.name.assign(data.begin() + pos, data.begin() + nullPos);
        pos = nullPos + 1;

        if (pos + 40 <= data.size()) {
            entry.hash.assign(data.begin() + pos, data.begin() + pos + 40);
            pos += 40;
        }
        entries.push_back(entry);
    }
    return entries;
}

} // namespace vcs
