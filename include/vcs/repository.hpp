#pragma once

#include "filesystem.hpp"
#include "object.hpp"
#include "index.hpp"
#include "refs.hpp"
#include "diff.hpp"
#include "merge.hpp"
#include "output.hpp"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <ctime>
#include <regex>
#include <fstream>

namespace vcs {

const std::string VCS_DIR = ".vcs";
const std::string VCS_IGNORE_FILE = ".vcsignore";

class VcsIgnore {
public:
    struct Pattern {
        std::regex re;
        bool negate{false};
        bool dirOnly{false};
    };

    void load(const std::string& filePath) {
        patterns_.clear();
        if (!FileSystem::exists(filePath)) return;
        std::ifstream f(filePath);
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            bool negate = false;
            bool dirOnly = false;
            std::string pat = line;

            if (pat[0] == '!') { negate = true; pat = pat.substr(1); }
            if (pat.back() == '/') { dirOnly = true; pat.pop_back(); }

            std::string reStr;
            for (size_t i = 0; i < pat.size(); ++i) {
                if (pat[i] == '*' && i + 1 < pat.size() && pat[i + 1] == '*') {
                    reStr += ".*";
                    while (i + 1 < pat.size() && pat[i + 1] == '*') ++i;
                } else if (pat[i] == '*') { reStr += "[^/]*";
                } else if (pat[i] == '?') { reStr += "[^/]";
                } else if (pat[i] == '.') { reStr += "\\.";
                } else { reStr += pat[i]; }
            }

            Pattern p;
            p.re = std::regex(reStr, std::regex::ECMAScript | std::regex::icase);
            p.negate = negate;
            p.dirOnly = dirOnly;
            patterns_.push_back(p);
        }
    }

    bool matches(const std::string& path, bool isDir = false) const {
        bool ignored = false;
        for (auto& p : patterns_) {
            if (p.dirOnly && !isDir) continue;
            if (std::regex_match(path, p.re)) {
                ignored = !p.negate;
            }
        }
        return ignored;
    }

private:
    std::vector<Pattern> patterns_;
};

class Repository {
public:
    explicit Repository(const std::string& path = "");

    bool init();
    static bool isRepository(const std::string& path = "");
    static std::string findVcsDir(const std::string& path = "");

    std::string vcsDir() const { return vcsDir_; }
    std::string workDir() const { return workDir_; }

    ObjectDatabase& objects() { return *odb_; }
    Reference& refs() { return *refs_; }
    Index& index() { return *index_; }
    VcsIgnore& ignorer() { return *vcsIgnore_; }

    std::vector<std::string> getUntrackedFiles() const;
    void printStatus();
    void printLog(const std::string& hash = "", int maxCount = -1) const;
    void printDiff(const std::string& target = "") const;

    std::string getStashHash() const;
    bool setStashHash(const std::string& hash) const;
    int gc();

    bool isIgnored(const std::string& path) const;

private:
    std::string vcsDir_;
    std::string workDir_;
    std::unique_ptr<ObjectDatabase> odb_;
    std::unique_ptr<Reference> refs_;
    std::unique_ptr<Index> index_;
    std::unique_ptr<VcsIgnore> vcsIgnore_;
    std::string getFileMode(const std::string& path) const;

    void scanDirectory(const std::string& dir, std::vector<TreeEntry>& entries,
                       const std::string& basePrefix = "") const;
    bool isModified(const std::string& path, const std::string& expectedHash) const;
};

inline Repository::Repository(const std::string& path) {
    auto basePath = path.empty() ? FileSystem::getCurrentWorkingDirectory() : path;
    auto found = findVcsDir(basePath);
    if (found.empty()) {
        workDir_ = fs::absolute(basePath).string();
        vcsDir_ = workDir_ + "/" + VCS_DIR;
    } else {
        vcsDir_ = found;
        workDir_ = fs::path(found).parent_path().string();
    }
    odb_ = std::make_unique<ObjectDatabase>(vcsDir_);
    refs_ = std::make_unique<Reference>(vcsDir_);
    index_ = std::make_unique<Index>(vcsDir_);
    vcsIgnore_ = std::make_unique<VcsIgnore>();
    vcsIgnore_->load(workDir_ + "/" + VCS_IGNORE_FILE);
}

inline bool Repository::init() {
    if (FileSystem::exists(vcsDir_)) return false;

    FileSystem::createDirectories(vcsDir_ + "/objects");
    FileSystem::createDirectories(vcsDir_ + "/refs/heads");
    FileSystem::createDirectories(vcsDir_ + "/refs/tags");

    FileSystem::writeTextFile(vcsDir_ + "/HEAD", "ref: refs/heads/master\n");
    FileSystem::writeTextFile(vcsDir_ + "/config",
        "[core]\n\trepositoryformatversion = 0\n\tfilemode = true\n\tbare = false\n");
    FileSystem::writeTextFile(vcsDir_ + "/description",
        "Unnamed repository; edit this file to name the repository.\n");

    index_->save();
    return true;
}

inline bool Repository::isRepository(const std::string& path) {
    return !findVcsDir(path).empty();
}

inline std::string Repository::findVcsDir(const std::string& path) {
    auto start = path.empty() ? FileSystem::getCurrentWorkingDirectory() : path;
    auto current = fs::absolute(start).string();

    while (true) {
        auto vcsPath = current + "/" + VCS_DIR;
        if (FileSystem::exists(vcsPath) && FileSystem::isDirectory(vcsPath)) {
            return fs::absolute(vcsPath).string();
        }
        auto parent = fs::path(current).parent_path().string();
        if (parent == current) break;
        current = parent;
    }
    return "";
}

inline bool Repository::isIgnored(const std::string& path) const {
    auto filename = fs::path(path).filename().string();
    if (filename == ".vcs" || filename == VCS_DIR) return true;
    if (filename == VCS_IGNORE_FILE) return true;
    if (filename == "vcs.exe" || filename == "vcs") return true;
    if (filename.empty()) return true;
    auto rel = fs::path(path).lexically_relative(workDir_).string();
    if (rel.empty()) rel = filename;
    bool isDir = FileSystem::isDirectory(path);
    if (vcsIgnore_ && vcsIgnore_->matches(rel, isDir)) return true;
    return false;
}

inline std::string Repository::getFileMode(const std::string& path) const {
    return "100644";
}

inline std::vector<std::string> Repository::getUntrackedFiles() const {
    std::vector<std::string> untracked;
    auto allFiles = FileSystem::listFiles(workDir_);
    index_->load();

    std::set<std::string> tracked;
    for (auto& e : index_->getAllFiles())
        tracked.insert(e.path);

    std::set<std::string> committed;
    auto headHash = refs_->resolveHead();
    if (!headHash.empty()) {
        try {
            auto headObj = odb_->read(headHash);
            std::string headData(headObj.content.begin(), headObj.content.end());
            std::istringstream iss(headData);
            std::string line, treeHash;
            while (std::getline(iss, line))
                if (line.substr(0, 5) == "tree ") { treeHash = line.substr(5); break; }
            if (!treeHash.empty()) {
                auto entries = odb_->parseTree(treeHash);
                for (auto& e : entries) committed.insert(e.name);
            }
        } catch (...) {}
    }

    for (auto& file : allFiles) {
        auto fullPath = workDir_ + "/" + file;
        if (isIgnored(fullPath)) continue;
        if (tracked.count(file) > 0) continue;
        if (committed.count(file) > 0) continue;
        if (!FileSystem::isRegularFile(fullPath)) continue;
        untracked.push_back(file);
    }

    return untracked;
}

inline bool Repository::isModified(const std::string& path,
                                    const std::string& expectedHash) const {
    auto fullPath = workDir_ + "/" + path;
    if (!FileSystem::exists(fullPath)) return true;
    auto content = FileSystem::readFile(fullPath);
    auto hash = SHA1::hash(std::vector<uint8_t>(content.begin(), content.end()));
    return hash != expectedHash;
}

inline void Repository::scanDirectory(const std::string& dir,
                                       std::vector<TreeEntry>& entries,
                                       const std::string& basePrefix) const {
    auto items = FileSystem::listDirectory(dir);
    for (auto& item : items) {
        auto fullPath = dir + "/" + item;
        if (isIgnored(fullPath)) continue;

        auto relPath = basePrefix.empty() ? item : basePrefix + "/" + item;

        if (FileSystem::isDirectory(fullPath)) {
            scanDirectory(fullPath, entries, relPath);
        } else if (FileSystem::isRegularFile(fullPath)) {
            auto content = FileSystem::readFile(fullPath);
            auto hash = odb_->storeBlob(content);
            TreeEntry te;
            te.mode = getFileMode(fullPath);
            te.name = relPath;
            te.hash = hash;
            entries.push_back(te);
        }
    }
}

inline void Repository::printStatus() {
    Output out;
    index_->load();
    auto staged = index_->getStagedFiles();
    auto headHash = refs_->resolveHead();

    out.header("On branch " + (refs_->getCurrentBranch().empty() ?
               "HEAD (detached)" : refs_->getCurrentBranch()));

    if (!headHash.empty()) {
        std::map<std::string, std::string> committedFiles;
        try {
            auto headObj = odb_->read(headHash);
            std::string headData(headObj.content.begin(), headObj.content.end());
            std::istringstream iss(headData);
            std::string line, treeHash;
            while (std::getline(iss, line))
                if (line.substr(0, 5) == "tree ") { treeHash = line.substr(5); break; }
            if (!treeHash.empty()) {
                auto entries = odb_->parseTree(treeHash);
                for (auto& e : entries) committedFiles[e.name] = e.hash;
            }
        } catch (...) {}

        std::map<std::string, std::string> indexFiles;
        for (auto& e : index_->getAllFiles())
            indexFiles[e.path] = e.hash;

        std::vector<std::string> modified;
        for (auto& [path, hash] : indexFiles) {
            auto fullPath = workDir_ + "/" + path;
            if (!FileSystem::exists(fullPath)) { modified.push_back(path); continue; }
            auto content = FileSystem::readFile(fullPath);
            auto curHash = SHA1::hash(
                std::vector<uint8_t>(content.begin(), content.end()));
            if (curHash != hash) modified.push_back(path);
        }

        if (staged.empty() && modified.empty()) {
            auto untracked = getUntrackedFiles();
            if (untracked.empty()) out.info("Nothing to commit, working tree clean");
            else out.info("Nothing to commit, working tree clean (untracked files present)");
        }

        if (!staged.empty()) {
            out.header("Changes to be committed:");
            for (auto& e : staged) {
                auto it = committedFiles.find(e.path);
                if (it == committedFiles.end())
                    out.println("  new file:   " + e.path, Output::GREEN);
                else if (it->second != e.hash)
                    out.println("  modified:   " + e.path, Output::GREEN);
                else
                    out.println("  staged:     " + e.path, Output::GREEN);
            }
        }

        if (!modified.empty()) {
            out.header("Changes not staged for commit:");
            for (auto& p : modified)
                out.println("  modified:   " + p, Output::RED);
        }
    } else {
        if (!staged.empty()) {
            out.header("Changes to be committed:");
            for (auto& e : staged)
                out.println("  new file:   " + e.path, Output::GREEN);
        } else {
            out.info("No commits yet");
        }
    }

    auto untracked = getUntrackedFiles();
    if (!untracked.empty()) {
        out.header("Untracked files:");
        for (auto& f : untracked)
            out.println("  " + f, Output::RED);
    }
}

inline void Repository::printLog(const std::string& hash, int maxCount) const {
    auto head = hash.empty() ? refs_->resolveHead() : hash;
    if (head.empty()) { std::cout << "No commits\n"; return; }

    std::string current = head;
    int count = 0;
    while (!current.empty() && (maxCount < 0 || count < maxCount)) {
        try {
            auto obj = odb_->read(current);
            if (obj.type != ObjectType::Commit) break;

            std::string data(obj.content.begin(), obj.content.end());
            std::istringstream iss(data);
            std::string line, treeHash, parentHash, author;
            std::string message;
            while (std::getline(iss, line)) {
                if (line.substr(0, 5) == "tree ") treeHash = line.substr(5);
                else if (line.substr(0, 7) == "parent ") parentHash = line.substr(7);
                else if (line.substr(0, 7) == "author ") author = line.substr(7);
                else if (line.empty()) { std::getline(iss, message, '\0'); break; }
            }

            std::cout << "\033[33mcommit " << Reference::hashToShort(current) << "\033[0m\n";
            if (!parentHash.empty())
                std::cout << "Parent:     " << Reference::hashToShort(parentHash) << "\n";
            std::cout << "Tree:       " << Reference::hashToShort(treeHash) << "\n";

            if (!author.empty()) std::cout << "Author:     " << author << "\n";
            std::cout << "\n    " << message << "\n\n";

            current = parentHash;
            ++count;
        } catch (...) { break; }
    }
}

inline void Repository::printDiff(const std::string& target) const {
    index_->load();
    auto headHash = refs_->resolveHead();

    if (target == "--staged") {
        if (!headHash.empty()) {
            try {
                auto headObj = odb_->read(headHash);
                std::string headData(headObj.content.begin(), headObj.content.end());
                std::istringstream iss(headData);
                std::string line, treeHash;
                while (std::getline(iss, line))
                    if (line.substr(0, 5) == "tree ") { treeHash = line.substr(5); break; }

                if (!treeHash.empty()) {
                    auto committedEntries = odb_->parseTree(treeHash);
                    std::map<std::string, std::string> committed;
                    for (auto& e : committedEntries) committed[e.name] = e.hash;

                    for (auto& e : index_->getStagedFiles()) {
                        auto it = committed.find(e.path);
                        if (it == committed.end() || it->second != e.hash) {
                            auto obj = odb_->read(e.hash);
                            auto content = std::string(obj.content.begin(), obj.content.end());
                            std::ostringstream empty;

                            auto oldContent = (it != committed.end())
                                ? std::string(odb_->read(it->second).content.begin(),
                                              odb_->read(it->second).content.end())
                                : "";

                            auto diffs = DiffEngine::diffLines(oldContent, content);
                            std::cout << "diff --staged a/" << e.path << " b/" << e.path << "\n";
                            printDiffToStdout(diffs);
                        }
                    }
                }
            } catch (...) {
                for (auto& e : index_->getStagedFiles()) {
                    auto obj = odb_->read(e.hash);
                    auto content = std::string(obj.content.begin(), obj.content.end());
                    auto diffs = DiffEngine::diffLines("", content);
                    std::cout << "diff --staged a/" << e.path << " b/" << e.path << "\n";
                    printDiffToStdout(diffs);
                }
            }
        } else {
            for (auto& e : index_->getStagedFiles()) {
                auto obj = odb_->read(e.hash);
                auto content = std::string(obj.content.begin(), obj.content.end());
                auto diffs = DiffEngine::diffLines("", content);
                std::cout << "diff --staged a/" << e.path << " b/" << e.path << "\n";
                printDiffToStdout(diffs);
            }
        }
    } else {
        for (auto& e : index_->getAllFiles()) {
            auto fullPath = workDir_ + "/" + e.path;
            if (!FileSystem::exists(fullPath)) continue;
            auto content = FileSystem::readFile(fullPath);
            auto curHash = SHA1::hash(
                std::vector<uint8_t>(content.begin(), content.end()));
            if (curHash != e.hash) {
                auto obj = odb_->read(e.hash);
                auto oldContent = std::string(obj.content.begin(), obj.content.end());
                auto newContent = std::string(content.begin(), content.end());
                auto diffs = DiffEngine::diffLines(oldContent, newContent);
                std::cout << "diff --git a/" << e.path << " b/" << e.path << "\n";
                printDiffToStdout(diffs);
            }
        }
    }
}

inline std::string Repository::getStashHash() const {
    auto path = vcsDir_ + "/refs/stash";
    if (!FileSystem::exists(path)) return "";
    return FileSystem::readTextFile(path);
}

inline bool Repository::setStashHash(const std::string& hash) const {
    return FileSystem::writeTextFile(vcsDir_ + "/refs/stash", hash);
}

inline int Repository::gc() {
    std::set<std::string> reachable;

    auto addCommit = [&](const std::string& hash, auto& self) -> void {
        if (hash.size() != 40 || reachable.count(hash)) return;
        reachable.insert(hash);
        try {
            auto obj = odb_->read(hash);
            if (obj.type != ObjectType::Commit) return;
            std::string data(obj.content.begin(), obj.content.end());
            std::istringstream iss(data);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.substr(0, 5) == "tree ") {
                    reachable.insert(line.substr(5));
                    try {
                        auto treeObj = odb_->read(line.substr(5));
                        if (treeObj.type == ObjectType::Tree) {
                            for (auto& e : odb_->parseTree(line.substr(5))) {
                                reachable.insert(e.hash);
                                try {
                                    auto blob = odb_->read(e.hash);
                                    if (blob.type == ObjectType::Tree) {
                                        for (auto& e2 : odb_->parseTree(e.hash))
                                            reachable.insert(e2.hash);
                                    }
                                } catch (...) {}
                            }
                        }
                    } catch (...) {}
                } else if (line.substr(0, 7) == "parent ") {
                    self(line.substr(7), self);
                }
            }
        } catch (...) {}
    };

    auto walkRef = [&](const std::string& hash) {
        if (hash.size() == 40) addCommit(hash, addCommit);
    };

    walkRef(refs_->resolveHead());

    auto stashHash = getStashHash();
    if (!stashHash.empty()) walkRef(stashHash);

    for (auto& b : refs_->listBranches())
        walkRef(refs_->getBranchRef(b));
    for (auto& t : refs_->listTags())
        walkRef(refs_->getTagRef(t));

    auto objectsDir = vcsDir_ + "/objects";
    int removed = 0;
    for (auto& dir : FileSystem::listDirectory(objectsDir)) {
        auto dirPath = objectsDir + "/" + dir;
        if (!FileSystem::isDirectory(dirPath)) continue;
        for (auto& file : FileSystem::listDirectory(dirPath)) {
            auto hash = dir + file;
            if (!reachable.count(hash)) {
                FileSystem::remove(dirPath + "/" + file);
                ++removed;
            }
        }
        if (FileSystem::listDirectory(dirPath).empty())
            FileSystem::remove(dirPath);
    }

    return removed;
}

} // namespace vcs
