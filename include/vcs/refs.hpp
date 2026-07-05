#pragma once

#include "filesystem.hpp"

#include <string>
#include <vector>

namespace vcs {

class Reference {
public:
    explicit Reference(const std::string& vcsDir);

    std::string getHead() const;
    bool setHead(const std::string& ref);
    std::string getCurrentBranch() const;
    bool setCurrentBranch(const std::string& branch);

    std::string getBranchRef(const std::string& branch) const;
    bool setBranchRef(const std::string& branch, const std::string& hash);
    bool deleteBranch(const std::string& branch);
    std::vector<std::string> listBranches() const;

    std::string getTagRef(const std::string& tag) const;
    bool setTagRef(const std::string& tag, const std::string& hash);
    bool deleteTag(const std::string& tag);
    std::vector<std::string> listTags() const;

    bool isHeadDetached() const;
    std::string resolveHead() const;
    std::string resolveRef(const std::string& ref) const;

    static std::string hashToShort(const std::string& hash);

private:
    std::string vcsDir_;
    std::string headFile() const { return vcsDir_ + "/HEAD"; }
    std::string refsHeadsDir() const { return vcsDir_ + "/refs/heads"; }
    std::string refsTagsDir() const { return vcsDir_ + "/refs/tags"; }
};

inline Reference::Reference(const std::string& vcsDir) : vcsDir_(vcsDir) {}

inline std::string Reference::getHead() const {
    if (!FileSystem::exists(headFile())) return "";
    auto content = FileSystem::readTextFile(headFile());
    auto trimPos = content.find_last_not_of(" \n\r\t");
    if (trimPos != std::string::npos) content = content.substr(0, trimPos + 1);
    return content;
}

inline bool Reference::setHead(const std::string& ref) {
    return FileSystem::writeTextFile(headFile(), ref);
}

inline std::string Reference::getCurrentBranch() const {
    auto head = getHead();
    const std::string prefix = "ref: refs/heads/";
    if (head.substr(0, prefix.size()) == prefix)
        return head.substr(prefix.size());
    return "";
}

inline bool Reference::setCurrentBranch(const std::string& branch) {
    return setHead("ref: refs/heads/" + branch);
}

inline std::string Reference::getBranchRef(const std::string& branch) const {
    auto path = refsHeadsDir() + "/" + branch;
    if (!FileSystem::exists(path)) return "";
    return FileSystem::readTextFile(path);
}

inline bool Reference::setBranchRef(const std::string& branch, const std::string& hash) {
    return FileSystem::writeTextFile(refsHeadsDir() + "/" + branch, hash);
}

inline bool Reference::deleteBranch(const std::string& branch) {
    return FileSystem::remove(refsHeadsDir() + "/" + branch);
}

inline std::vector<std::string> Reference::listBranches() const {
    return FileSystem::listFiles(refsHeadsDir());
}

inline std::string Reference::getTagRef(const std::string& tag) const {
    auto path = refsTagsDir() + "/" + tag;
    if (!FileSystem::exists(path)) return "";
    return FileSystem::readTextFile(path);
}

inline bool Reference::setTagRef(const std::string& tag, const std::string& hash) {
    return FileSystem::writeTextFile(refsTagsDir() + "/" + tag, hash);
}

inline bool Reference::deleteTag(const std::string& tag) {
    return FileSystem::remove(refsTagsDir() + "/" + tag);
}

inline std::vector<std::string> Reference::listTags() const {
    return FileSystem::listFiles(refsTagsDir());
}

inline bool Reference::isHeadDetached() const {
    auto head = getHead();
    return !head.empty() && head.substr(0, 4) != "ref:";
}

inline std::string Reference::resolveHead() const {
    auto head = getHead();
    if (head.empty()) return "";
    const std::string prefix = "ref: ";
    if (head.substr(0, prefix.size()) == prefix) {
        auto refPath = vcsDir_ + "/" + head.substr(prefix.size());
        if (FileSystem::exists(refPath)) return FileSystem::readTextFile(refPath);
        auto trimmed = head.substr(prefix.size());
        auto newlinePos = trimmed.find_last_not_of(" \n\r\t");
        if (newlinePos != std::string::npos) trimmed = trimmed.substr(0, newlinePos + 1);
        refPath = vcsDir_ + "/" + trimmed;
        if (FileSystem::exists(refPath)) return FileSystem::readTextFile(refPath);
        return "";
    }
    auto trimmed = head;
    auto newlinePos = trimmed.find_last_not_of(" \n\r\t");
    if (newlinePos != std::string::npos) trimmed = trimmed.substr(0, newlinePos + 1);
    return trimmed;
}

inline std::string Reference::resolveRef(const std::string& ref) const {
    if (ref.size() == 40) return ref;
    if (ref == "HEAD") return resolveHead();
    auto branchPath = refsHeadsDir() + "/" + ref;
    if (FileSystem::exists(branchPath)) return FileSystem::readTextFile(branchPath);
    auto tagPath = refsTagsDir() + "/" + ref;
    if (FileSystem::exists(tagPath)) return FileSystem::readTextFile(tagPath);
    return "";
}

inline std::string Reference::hashToShort(const std::string& hash) {
    return hash.size() >= 7 ? hash.substr(0, 7) : hash;
}

} // namespace vcs
