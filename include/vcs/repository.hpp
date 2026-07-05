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
#include <functional>
#include <cstring>

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
    bool reset(const std::string& targetHash, const std::string& mode);
    bool cherryPick(const std::string& commitHash);
    bool rebase(const std::string& targetBranch);
    std::vector<std::string> getParentChain(const std::string& startHash) const;

    int fsck();
    bool archive(const std::string& commitHash, const std::string& outputFile);
    std::string formatPatch(const std::string& commitHash);
    bool am(const std::string& patchFile, const std::string& author = "");

    bool remoteAdd(const std::string& name, const std::string& url);
    std::string remoteUrl(const std::string& name) const;
    std::vector<std::string> remoteList() const;
    bool clone(const std::string& url, const std::string& dir);
    bool fetch(const std::string& remote);
    bool push(const std::string& remote);
    bool pull(const std::string& remote);

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

inline std::vector<std::string> Repository::getParentChain(const std::string& startHash) const {
    std::vector<std::string> result;
    std::string current = startHash;
    while (!current.empty()) {
        result.push_back(current);
        try {
            auto obj = odb_->read(current);
            if (obj.type != ObjectType::Commit) break;
            std::string data(obj.content.begin(), obj.content.end());
            std::istringstream iss(data);
            std::string line, parent;
            bool found = false;
            while (std::getline(iss, line)) {
                if (line.substr(0, 7) == "parent ") { parent = line.substr(7); found = true; break; }
            }
            if (!found) break;
            current = parent;
        } catch (...) { break; }
    }
    return result;
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

static std::map<std::string, std::string> getTreeFileMap(ObjectDatabase& odb, const std::string& commitHash) {
    std::map<std::string, std::string> files;
    if (commitHash.empty()) return files;
    try {
        auto obj = odb.read(commitHash);
        if (obj.type != ObjectType::Commit) return files;
        std::string data(obj.content.begin(), obj.content.end());
        std::istringstream iss(data);
        std::string line, treeHash;
        while (std::getline(iss, line))
            if (line.substr(0, 5) == "tree ") { treeHash = line.substr(5); break; }
        if (!treeHash.empty()) {
            for (auto& e : odb.parseTree(treeHash)) files[e.name] = e.hash;
        }
    } catch (...) {}
    return files;
}

inline bool Repository::cherryPick(const std::string& commitHash) {
    try {
        auto commitObj = odb_->read(commitHash);
        if (commitObj.type != ObjectType::Commit) return false;

        std::string data(commitObj.content.begin(), commitObj.content.end());
        std::istringstream iss(data);
        std::string line, treeHash, parentHash, message;
        while (std::getline(iss, line)) {
            if (line.substr(0, 5) == "tree ") treeHash = line.substr(5);
            else if (line.substr(0, 7) == "parent ") parentHash = line.substr(7);
            else if (line.empty()) { std::getline(iss, message, '\0'); break; }
        }
        if (treeHash.empty()) return false;

        auto headHash = refs_->resolveHead();

        auto commitFiles = getTreeFileMap(*odb_, commitHash);
        auto parentFiles = getTreeFileMap(*odb_, parentHash);
        auto headFiles = getTreeFileMap(*odb_, headHash);

        index_->load();

        std::set<std::string> allFiles;
        for (auto& [k, _] : commitFiles) allFiles.insert(k);
        for (auto& [k, _] : parentFiles) allFiles.insert(k);
        for (auto& [k, _] : headFiles) allFiles.insert(k);

        bool conflict = false;

        for (auto& f : allFiles) {
            auto cf = commitFiles.find(f);
            auto pf = parentFiles.find(f);
            auto hf = headFiles.find(f);
            auto curHash = (cf != commitFiles.end()) ? cf->second : "";
            auto ancHash = (pf != parentFiles.end()) ? pf->second : "";
            auto ourHash = (hf != headFiles.end()) ? hf->second : "";

            if (curHash == ourHash) continue;
            if (curHash == ancHash) continue;

            auto fullPath = workDir_ + "/" + f;

            if (ancHash == ourHash || ancHash.empty()) {
                if (curHash.empty()) {
                    if (FileSystem::exists(fullPath)) FileSystem::remove(fullPath);
                    index_->unstage(f);
                } else {
                    auto obj = odb_->read(curHash);
                    FileSystem::writeFile(fullPath, obj.content);
                    auto mtime = FileSystem::getLastModifiedTime(fullPath);
                    auto size = FileSystem::getFileSize(fullPath);
                    index_->stage(f, curHash, mtime, size);
                }
            } else {
                std::string ancStr, ourStr, curStr;
                if (!ancHash.empty()) {
                    auto ancObj2 = odb_->read(ancHash);
                    ancStr = std::string(ancObj2.content.begin(), ancObj2.content.end());
                }
                if (!ourHash.empty()) {
                    auto ourObj2 = odb_->read(ourHash);
                    ourStr = std::string(ourObj2.content.begin(), ourObj2.content.end());
                }
                if (!curHash.empty()) {
                    auto curObj2 = odb_->read(curHash);
                    curStr = std::string(curObj2.content.begin(), curObj2.content.end());
                }

                std::ofstream fout(fullPath);
                if (fout) {
                    fout << "<<<<<<< HEAD (current)\n";
                    fout << ourStr;
                    if (!ourStr.empty() && ourStr.back() != '\n') fout << '\n';
                    fout << "=======\n";
                    fout << curStr;
                    if (!curStr.empty() && curStr.back() != '\n') fout << '\n';
                    fout << ">>>>>>> cherry-pick (" << Reference::hashToShort(commitHash) << ")\n";
                }
                conflict = true;
                std::cerr << "\033[31mConflict: " << f << "\033[0m\n";
            }
        }

        index_->save();

        if (conflict) {
            std::cerr << "\033[33mCherry-pick caused conflicts. Resolve them, then 'vcs add' and 'vcs commit'.\033[0m\n";
            return false;
        }

        auto newTreeHash = [&]() {
            std::vector<TreeEntry> entries;
            for (auto& e : index_->getAllFiles()) {
                TreeEntry te;
                te.mode = "100644";
                te.name = e.path;
                te.hash = e.hash;
                entries.push_back(te);
            }
            return odb_->storeTree(entries);
        }();

        std::string author = "Author <author@vcs.local>";
        auto newCommit = odb_->storeCommit(newTreeHash, headHash, author, message);

        auto branch = refs_->getCurrentBranch();
        if (!branch.empty()) refs_->setBranchRef(branch, newCommit);
        else refs_->setHead(newCommit);

        return true;
    } catch (std::exception& e) {
        std::cerr << "\033[31mCherry-pick exception: " << e.what() << "\033[0m\n";
        return false;
    } catch (...) {
        std::cerr << "\033[31mCherry-pick unknown exception\033[0m\n";
        return false;
    }
}

inline bool Repository::reset(const std::string& targetHash, const std::string& mode) {
    try {
        auto commitObj = odb_->read(targetHash);
        if (commitObj.type != ObjectType::Commit) return false;

        std::string data(commitObj.content.begin(), commitObj.content.end());
        std::istringstream iss(data);
        std::string line, treeHash;
        while (std::getline(iss, line))
            if (line.substr(0, 5) == "tree ") { treeHash = line.substr(5); break; }

        if (treeHash.empty()) return false;

        auto treeEntries = odb_->parseTree(treeHash);

        index_->load();

        if (mode == "soft") {
        } else if (mode == "mixed") {
            index_->clear();
            for (auto& e : treeEntries) {
                uint64_t mtime = 0, size = 0;
                auto fullPath = workDir_ + "/" + e.name;
                if (FileSystem::exists(fullPath)) {
                    mtime = FileSystem::getLastModifiedTime(fullPath);
                    size = FileSystem::getFileSize(fullPath);
                }
                index_->stage(e.name, e.hash, mtime, size);
            }
            index_->save();
        } else if (mode == "hard") {
            index_->clear();
            for (auto& e : treeEntries) {
                auto fullPath = workDir_ + "/" + e.name;
                try {
                    auto obj = odb_->read(e.hash);
                    FileSystem::writeFile(fullPath, obj.content);
                } catch (...) {}
                auto mtime = FileSystem::getLastModifiedTime(fullPath);
                auto size = FileSystem::getFileSize(fullPath);
                index_->stage(e.name, e.hash, mtime, size);
            }
            index_->save();
        }

        auto branch = refs_->getCurrentBranch();
        if (!branch.empty()) {
            refs_->setBranchRef(branch, targetHash);
        } else {
            refs_->setHead(targetHash);
        }

        return true;
    } catch (...) { return false; }
}

inline bool Repository::rebase(const std::string& targetBranch) {
    auto headHash = refs_->resolveHead();
    if (headHash.empty()) return false;

    auto targetHash = refs_->getBranchRef(targetBranch);
    if (targetHash.empty()) return false;

    if (headHash == targetHash) return true;

    auto headChain = getParentChain(headHash);
    auto targetChain = getParentChain(targetHash);

    std::string forkPoint;
    for (auto& h : headChain) {
        for (auto& t : targetChain) {
            if (h == t) { forkPoint = h; break; }
        }
        if (!forkPoint.empty()) break;
    }

    std::vector<std::string> toReplay;
    for (auto& h : headChain) {
        if (h == forkPoint) break;
        toReplay.push_back(h);
    }
    std::reverse(toReplay.begin(), toReplay.end());

    if (toReplay.empty()) return true;

    if (!reset(targetHash, "hard")) return false;

    for (auto& c : toReplay) {
        if (!cherryPick(c)) return false;
    }

    return true;
}

inline int Repository::fsck() {
    Output out;
    int checked = 0, errors = 0, dangling = 0;
    std::set<std::string> reachable;

    auto addReachable = [&](const std::string& hash, auto& self) -> void {
        if (hash.size() != 40 || reachable.count(hash)) return;
        reachable.insert(hash);
        try {
            auto obj = odb_->read(hash);
            if (obj.type == ObjectType::Commit) {
                std::string d(obj.content.begin(), obj.content.end());
                std::istringstream iss(d);
                std::string line;
                while (std::getline(iss, line)) {
                    if (line.substr(0, 5) == "tree ") { reachable.insert(line.substr(5)); self(line.substr(5), self); }
                    else if (line.substr(0, 7) == "parent ") self(line.substr(7), self);
                }
            } else if (obj.type == ObjectType::Tree) {
                for (auto& e : odb_->parseTree(hash)) { reachable.insert(e.hash); }
            }
        } catch (...) {}
    };

    addReachable(refs_->resolveHead(), addReachable);
    if (!getStashHash().empty()) addReachable(getStashHash(), addReachable);
    for (auto& b : refs_->listBranches()) {
        auto h = refs_->getBranchRef(b);
        if (!h.empty()) addReachable(h, addReachable);
    }
    for (auto& t : refs_->listTags()) {
        auto h = refs_->getTagRef(t);
        if (!h.empty()) addReachable(h, addReachable);
    }

    auto objectsDir = vcsDir_ + "/objects";
    for (auto& dir : FileSystem::listDirectory(objectsDir)) {
        auto dirPath = objectsDir + "/" + dir;
        if (!FileSystem::isDirectory(dirPath)) continue;
        for (auto& file : FileSystem::listDirectory(dirPath)) {
            auto hash = dir + file;
            ++checked;
            try {
                auto path = dirPath + "/" + file;
                auto compressed = FileSystem::readFile(path);
                auto raw = Compression::decompress(compressed);
                auto obj = Object::deserialize(raw);
                auto computedHash = obj.computeHash();
                if (computedHash != hash) {
                    out.error("Hash mismatch: " + hash + " vs " + computedHash);
                    ++errors;
                }
                if (reachable.find(hash) == reachable.end()) {
                    ++dangling;
                }
            } catch (std::exception& e) {
                out.error("Corrupt object " + hash + ": " + e.what());
                ++errors;
            }
        }
    }

    out.info("Checked " + std::to_string(checked) + " objects");
    if (errors) out.error(std::to_string(errors) + " error(s) found");
    else out.success("No errors found");
    if (dangling) out.warn(std::to_string(dangling) + " dangling object(s)");
    return errors;
}

inline bool Repository::archive(const std::string& commitHash, const std::string& outputFile) {
    try {
        auto obj = odb_->read(commitHash);
        if (obj.type != ObjectType::Commit) return false;
        std::string d(obj.content.begin(), obj.content.end());
        std::istringstream iss(d);
        std::string line, treeHash;
        while (std::getline(iss, line))
            if (line.substr(0, 5) == "tree ") { treeHash = line.substr(5); break; }
        if (treeHash.empty()) return false;

        std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
        std::function<void(const std::string&, const std::string&)> walk =
            [&](const std::string& th, const std::string& prefix) {
                for (auto& e : odb_->parseTree(th)) {
                    auto fullName = prefix.empty() ? e.name : prefix + "/" + e.name;
                    try {
                        auto blob = odb_->read(e.hash);
                        if (blob.type == ObjectType::Tree) walk(e.hash, fullName);
                        else files.push_back({fullName, blob.content});
                    } catch (...) {}
                }
            };
        walk(treeHash, "");

        std::ofstream out(outputFile, std::ios::binary);
        if (!out) return false;

        auto writeLE16 = [&](uint16_t v) { out.put(v & 0xFF); out.put((v >> 8) & 0xFF); };
        auto writeLE32 = [&](uint32_t v) {
            out.put(v & 0xFF); out.put((v >> 8) & 0xFF);
            out.put((v >> 16) & 0xFF); out.put((v >> 24) & 0xFF);
        };

        auto dosTime = [](time_t t) -> uint16_t {
            struct tm* tm = localtime(&t);
            if (!tm) return 0;
            return (tm->tm_sec / 2) | (tm->tm_min << 5) | (tm->tm_hour << 11);
        };
        auto dosDate = [](time_t t) -> uint16_t {
            struct tm* tm = localtime(&t);
            if (!tm) return 0;
            return tm->tm_mday | ((tm->tm_mon + 1) << 5) | ((tm->tm_year - 80) << 9);
        };

        std::vector<size_t> offsets;
        time_t now = std::time(nullptr);
        for (auto& [name, content] : files) {
            offsets.push_back(static_cast<size_t>(out.tellp()));
            auto crc = Compression::crc32(content);
            auto compressed = Compression::deflate(content);

            // Local file header
            out.write("PK\x03\x04", 4);
            writeLE16(20); // version needed
            writeLE16(0);  // flags
            writeLE16(8);  // deflate
            writeLE16(dosTime(now));
            writeLE16(dosDate(now));
            writeLE32(crc);
            writeLE32(static_cast<uint32_t>(compressed.size()));
            writeLE32(static_cast<uint32_t>(content.size()));
            writeLE16(static_cast<uint16_t>(name.size()));
            writeLE16(0);  // extra
            out.write(name.data(), name.size());
            out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
        }

        size_t centralOffset = static_cast<size_t>(out.tellp());
        uint16_t entryCount = static_cast<uint16_t>(files.size());
        for (size_t i = 0; i < files.size(); ++i) {
            auto& [name, content] = files[i];
            auto crc = Compression::crc32(content);
            auto compressed = Compression::deflate(content);

            out.write("PK\x01\x02", 4);
            writeLE16(20); writeLE16(20); writeLE16(0);
            writeLE16(8);  writeLE16(dosTime(now));
            writeLE16(dosDate(now)); writeLE32(crc);
            writeLE32(static_cast<uint32_t>(compressed.size()));
            writeLE32(static_cast<uint32_t>(content.size()));
            writeLE16(static_cast<uint16_t>(name.size()));
            writeLE16(0); writeLE16(0); writeLE16(0);
            writeLE16(0); writeLE32(0);
            writeLE32(static_cast<uint32_t>(offsets[i]));
            out.write(name.data(), name.size());
        }

        // End of central directory
        out.write("PK\x05\x06", 4);
        writeLE16(0); writeLE16(0);
        writeLE16(entryCount); writeLE16(entryCount);
        writeLE32(static_cast<uint32_t>(static_cast<size_t>(out.tellp()) - centralOffset));
        writeLE32(static_cast<uint32_t>(centralOffset));
        writeLE16(0);

        return true;
    } catch (...) { return false; }
}

inline std::string Repository::formatPatch(const std::string& commitHash) {
    try {
        auto obj = odb_->read(commitHash);
        if (obj.type != ObjectType::Commit) return "";
        std::string data(obj.content.begin(), obj.content.end());
        std::istringstream iss(data);
        std::string line, treeHash, parentHash, author, message;
        while (std::getline(iss, line)) {
            if (line.substr(0, 5) == "tree ") treeHash = line.substr(5);
            else if (line.substr(0, 7) == "parent ") parentHash = line.substr(7);
            else if (line.substr(0, 7) == "author ") author = line.substr(7);
            else if (line.empty()) { std::getline(iss, message, '\0'); break; }
        }
        if (treeHash.empty()) return "";

        std::map<std::string, std::string> commitFiles, parentFiles;
        for (auto& e : odb_->parseTree(treeHash)) commitFiles[e.name] = e.hash;
        if (!parentHash.empty()) {
            try {
                auto pObj = odb_->read(parentHash);
                std::string pd(pObj.content.begin(), pObj.content.end());
                std::istringstream pIss(pd);
                std::string pl, pt;
                while (std::getline(pIss, pl))
                    if (pl.substr(0, 5) == "tree ") { pt = pl.substr(5); break; }
                if (!pt.empty()) for (auto& e : odb_->parseTree(pt)) parentFiles[e.name] = e.hash;
            } catch (...) {}
        }

        std::ostringstream patch;
        patch << "From " << commitHash << " Mon Sep 17 00:00:00 2001\n";
        patch << "From: " << author << "\n";
        patch << "Date: " << std::time(nullptr) << "\n";
        patch << "Subject: " << message << "\n\n";

        std::set<std::string> allFiles;
        for (auto& [k, _] : commitFiles) allFiles.insert(k);
        for (auto& [k, _] : parentFiles) allFiles.insert(k);

        for (auto& f : allFiles) {
            auto cf = commitFiles.find(f);
            auto pf = parentFiles.find(f);
            auto curHash = (cf != commitFiles.end()) ? cf->second : "";
            auto oldHash = (pf != parentFiles.end()) ? pf->second : "";

            if (curHash == oldHash) continue;

            std::string oldContent, newContent;
            if (!oldHash.empty()) oldContent.assign(odb_->read(oldHash).content.begin(), odb_->read(oldHash).content.end());
            if (!curHash.empty()) newContent.assign(odb_->read(curHash).content.begin(), odb_->read(curHash).content.end());

            patch << "diff --git a/" << f << " b/" << f << "\n";
            patch << "--- a/" << f << "\n" << "+++ b/" << f << "\n";

            auto diffs = DiffEngine::diffLines(oldContent, newContent);
            for (auto& d : diffs) {
                switch (d.type) {
                    case DiffLine::CONTEXT:  patch << " " << d.content; break;
                    case DiffLine::ADDITION: patch << "+" << d.content; break;
                    case DiffLine::DELETION:  patch << "-" << d.content; break;
                }
            }
        }

        return patch.str();
    } catch (...) { return ""; }
}

inline bool Repository::am(const std::string& patchFile, const std::string& author) {
    if (!FileSystem::exists(patchFile)) return false;
    auto content = FileSystem::readTextFile(patchFile);
    std::istringstream iss(content);
    std::string line, subject, from;
    bool inBody = false;
    std::vector<std::string> diffSections;
    std::string currentDiff;
    bool inDiff = false;

    while (std::getline(iss, line)) {
        if (line.substr(0, 7) == "Subject") { subject = line.substr(8); }
        else if (line.substr(0, 5) == "From ") { /* skip mail header */ }
        else if (line.substr(0, 6) == "From: ") { from = line.substr(6); }
        else if (line.substr(0, 10) == "diff --git") {
            if (!currentDiff.empty()) diffSections.push_back(currentDiff);
            currentDiff = line + "\n";
            inDiff = true;
        } else if (inDiff) {
            currentDiff += line + "\n";
        }
    }
    if (!currentDiff.empty()) diffSections.push_back(currentDiff);

    index_->load();
    for (auto& sec : diffSections) {
        std::istringstream sIss(sec);
        std::string dl, oldFile, newFile, oldName;
        std::vector<std::string> oldLines, newLines;
        bool parsingOld = false, parsingNew = false;

        while (std::getline(sIss, dl)) {
            if (dl.substr(0, 10) == "diff --git") {
                auto space = dl.find_last_of(' ');
                if (space != std::string::npos) newFile = dl.substr(space + 1);
                continue;
            }
            if (dl.substr(0, 4) == "--- ") { oldFile = dl.substr(6); continue; }
            if (dl.substr(0, 4) == "+++ ") { newFile = dl.substr(6); continue; }
            if (dl.empty() || dl[0] == ' ') { parsingOld = true; parsingNew = true; continue; }
            if (dl[0] == '-') oldLines.push_back(dl.substr(1));
            else if (dl[0] == '+') newLines.push_back(dl.substr(1));
            else if (dl[0] == ' ') { oldLines.push_back(dl.substr(1)); newLines.push_back(dl.substr(1)); }
        }

        auto filePath = newFile;
        auto fullPath = workDir_ + "/" + filePath;
        auto dir = fs::path(fullPath).parent_path().string();
        if (!FileSystem::exists(dir)) FileSystem::createDirectories(dir);

        std::ostringstream fout;
        for (auto& l : newLines) fout << l << "\n";
        auto fileContent = fout.str();
        if (!fileContent.empty() && fileContent.back() == '\n') fileContent.pop_back();

        auto blobHash = odb_->storeBlob(std::vector<uint8_t>(fileContent.begin(), fileContent.end()));
        FileSystem::writeFile(fullPath, std::vector<uint8_t>(fileContent.begin(), fileContent.end()));
        auto mtime = FileSystem::getLastModifiedTime(fullPath);
        auto size = FileSystem::getFileSize(fullPath);
        index_->stage(filePath, blobHash, mtime, size);
    }
    index_->save();

    std::vector<TreeEntry> treeEntries;
    for (auto& e : index_->getAllFiles()) {
        TreeEntry te;
        te.mode = "100644";
        te.name = e.path;
        te.hash = e.hash;
        treeEntries.push_back(te);
    }
    auto treeHash = odb_->storeTree(treeEntries);
    auto headHash = refs_->resolveHead();
    auto commitAuthor = author.empty() ? "Author <author@vcs.local>" : author;
    auto newCommit = odb_->storeCommit(treeHash, headHash, commitAuthor, subject);
    auto branch = refs_->getCurrentBranch();
    if (!branch.empty()) refs_->setBranchRef(branch, newCommit);
    else refs_->setHead(newCommit);

    return true;
}

inline bool Repository::remoteAdd(const std::string& name, const std::string& url) {
    auto remotesDir = vcsDir_ + "/remotes";
    FileSystem::createDirectories(remotesDir);
    return FileSystem::writeTextFile(remotesDir + "/" + name, url);
}

inline std::string Repository::remoteUrl(const std::string& name) const {
    auto path = vcsDir_ + "/remotes/" + name;
    if (!FileSystem::exists(path)) return "";
    return FileSystem::readTextFile(path);
}

inline std::vector<std::string> Repository::remoteList() const {
    auto remotesDir = vcsDir_ + "/remotes";
    if (!FileSystem::exists(remotesDir)) return {};
    return FileSystem::listFiles(remotesDir);
}

inline bool Repository::clone(const std::string& url, const std::string& dir) {
    fs::path target(dir);
    if (FileSystem::exists(target.string())) return false;
    auto srcVcs = fs::path(url) / VCS_DIR;
    if (!FileSystem::exists(srcVcs.string())) return false;

    init();
    auto dstVcs = vcsDir_;

    auto copyDir = [&](const fs::path& src, const fs::path& dst, auto& self) -> void {
        if (!FileSystem::exists(dst.string())) FileSystem::createDirectories(dst.string());
        for (auto& e : fs::directory_iterator(src)) {
            auto target = dst / e.path().filename();
            if (e.is_directory()) self(e.path(), target, self);
            else fs::copy_file(e.path(), target, fs::copy_options::overwrite_existing);
        }
    };
    copyDir(srcVcs, fs::path(dstVcs), copyDir);

    refs_->setCurrentBranch(refs_->getCurrentBranch());

    auto headHash = refs_->resolveHead();
    if (!headHash.empty()) reset(headHash, "hard");
    return true;
}

inline bool Repository::fetch(const std::string& remote) {
    auto url = remoteUrl(remote);
    if (url.empty()) return false;
    auto remoteDir = fs::path(url) / VCS_DIR;
    if (!FileSystem::exists(remoteDir.string())) return false;

    auto copyDir = [&](const fs::path& src, const fs::path& dst, auto& self) -> void {
        if (!FileSystem::exists(dst.string())) FileSystem::createDirectories(dst.string());
        for (auto& e : fs::directory_iterator(src)) {
            auto target = dst / e.path().filename();
            if (e.is_directory()) self(e.path(), target, self);
            else if (!fs::exists(target)) fs::copy_file(e.path(), target);
        }
    };
    copyDir(remoteDir / "objects", fs::path(vcsDir_) / "objects", copyDir);

    auto refsDir = remoteDir / "refs";
    if (FileSystem::exists(refsDir.string())) {
        copyDir(refsDir / "heads", fs::path(vcsDir_) / "refs" / "remotes" / remote, copyDir);
    }

    return true;
}

inline bool Repository::push(const std::string& remote) {
    auto url = remoteUrl(remote);
    if (url.empty()) return false;
    auto remoteDir = fs::path(url) / VCS_DIR;
    if (!FileSystem::exists(remoteDir.string())) return false;

    auto copyDir = [&](const fs::path& src, const fs::path& dst, auto& self) -> void {
        if (!FileSystem::exists(dst.string())) FileSystem::createDirectories(dst.string());
        for (auto& e : fs::directory_iterator(src)) {
            auto target = dst / e.path().filename();
            if (e.is_directory()) self(e.path(), target, self);
            else if (!fs::exists(target)) fs::copy_file(e.path(), target);
        }
    };
    copyDir(fs::path(vcsDir_) / "objects", remoteDir / "objects", copyDir);

    for (auto& b : refs_->listBranches()) {
        auto hash = refs_->getBranchRef(b);
        if (!hash.empty()) FileSystem::writeTextFile((remoteDir / "refs/heads" / b).string(), hash);
    }
    for (auto& t : refs_->listTags()) {
        auto hash = refs_->getTagRef(t);
        if (!hash.empty()) FileSystem::writeTextFile((remoteDir / "refs/tags" / t).string(), hash);
    }

    return true;
}

inline bool Repository::pull(const std::string& remote) {
    if (!fetch(remote)) return false;
    auto remoteBranchPath = fs::path(vcsDir_) / "refs" / "remotes" / remote / refs_->getCurrentBranch();
    if (!FileSystem::exists(remoteBranchPath.string())) return false;
    auto remoteHash = FileSystem::readTextFile(remoteBranchPath.string());
    if (remoteHash.empty()) return false;

    auto headHash = refs_->resolveHead();
    if (headHash.empty()) {
        reset(remoteHash, "hard");
        return true;
    }

    MergeEngine me(*odb_, *index_, workDir_);
    auto result = me.merge(headHash, remoteHash, refs_->getCurrentBranch(), remote + "/" + refs_->getCurrentBranch());
    if (!result.success) return false;

    auto branch = refs_->getCurrentBranch();
    if (!branch.empty()) {
        auto mergeCommit = odb_->storeCommit(result.mergeHash, headHash, "Merge <merge@vcs.local>", "Merge " + remote + "/" + branch);
        refs_->setBranchRef(branch, mergeCommit);
    }
    return true;
}

} // namespace vcs
