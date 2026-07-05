#pragma once

#include "object.hpp"
#include "index.hpp"
#include "diff.hpp"
#include "filesystem.hpp"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace vcs {

class MergeEngine {
public:
    struct MergeResult {
        bool success{false};
        bool fastForward{false};
        std::string mergeHash;
        std::string message;
    };

    MergeEngine(ObjectDatabase& odb, Index& idx, const std::string& workDir)
        : odb_(odb), idx_(idx), workDir_(workDir) {}

    MergeResult merge(const std::string& currentHash,
                      const std::string& otherHash,
                      const std::string& currentBranch,
                      const std::string& otherBranch) {
        MergeResult result;

        if (currentHash.empty() || otherHash.empty()) return result;

        if (currentHash == otherHash) {
            result.success = true;
            result.message = "Already up to date";
            return result;
        }

        auto ancestor = findCommonAncestor(currentHash, otherHash);

        if (ancestor == otherHash) {
            result.success = true;
            result.fastForward = true;
            result.message = "Fast-forward";
            return result;
        }

        if (ancestor == currentHash) {
            result.fastForward = true;
            result.success = true;
            result.message = "Fast-forward";
            return result;
        }

        auto currentTree = getTreeHash(currentHash);
        auto otherTree = getTreeHash(otherHash);
        auto ancestorTree = getTreeHash(ancestor);

        if (currentTree.empty() || otherTree.empty()) return result;

        auto currentFiles = getTreeFiles(currentTree);
        auto otherFiles = getTreeFiles(otherTree);
        auto ancestorFiles = ancestorTree.empty() ?
            std::map<std::string, std::string>() : getTreeFiles(ancestorTree);

        std::map<std::string, std::string> merged;
        std::set<std::string> allFiles;
        for (auto& [k, _] : currentFiles) allFiles.insert(k);
        for (auto& [k, _] : otherFiles) allFiles.insert(k);
        for (auto& [k, _] : ancestorFiles) allFiles.insert(k);
        for (auto& k : allFiles) {
            auto cur = currentFiles.find(k);
            auto oth = otherFiles.find(k);
            auto anc = ancestorFiles.find(k);

            auto curHash = (cur != currentFiles.end()) ? cur->second : "";
            auto othHash = (oth != otherFiles.end()) ? oth->second : "";
            auto ancHash = (anc != ancestorFiles.end()) ? anc->second : "";

            if (curHash == othHash) {
                if (!curHash.empty()) merged[k] = curHash;
            } else if (curHash == ancHash) {
                if (!othHash.empty()) merged[k] = othHash;
            } else if (othHash == ancHash) {
                if (!curHash.empty()) merged[k] = curHash;
            } else if (ancHash.empty()) {
                merged[k] = othHash;
            } else {
                try {
                    resolveConflict(k, curHash, othHash, result);
                } catch (...) {}
                if (!othHash.empty()) merged[k] = othHash;
            }
        }
        if (!result.success) {
            result.message = "Merge conflict";
            return result;
        }

        idx_.load();
        for (auto& [name, hash] : merged) {
            auto obj = odb_.read(hash);
            auto fullPath = workDir_ + "/" + name;
            FileSystem::writeFile(fullPath, obj.content);
            auto mtime = FileSystem::getLastModifiedTime(fullPath);
            auto size = FileSystem::getFileSize(fullPath);
            idx_.stage(name, hash, mtime, size);
        }
        idx_.save();

        auto newTreeHash = buildTree(merged);
        result.mergeHash = newTreeHash;
        result.success = true;
        result.message = "Merged " + otherBranch + " into " + currentBranch;
        return result;
    }

    std::string findCommonAncestor(const std::string& a, const std::string& b) {
        std::set<std::string> visited;
        std::string current = a;
        while (!current.empty()) {
            visited.insert(current);
            auto parents = getParents(current);
            if (parents.empty()) break;
            current = parents[0];
        }
        current = b;
        while (!current.empty()) {
            if (visited.find(current) != visited.end()) return current;
            auto parents = getParents(current);
            if (parents.empty()) break;
            current = parents[0];
        }
        return "";
    }

private:
    ObjectDatabase& odb_;
    Index& idx_;
    std::string workDir_;

    std::string getTreeHash(const std::string& commitHash) {
        try {
            auto obj = odb_.read(commitHash);
            if (obj.type != ObjectType::Commit) return "";
            std::string data(obj.content.begin(), obj.content.end());
            std::istringstream iss(data);
            std::string line;
            while (std::getline(iss, line))
                if (line.substr(0, 5) == "tree ") return line.substr(5);
        } catch (...) {}
        return "";
    }

    std::vector<std::string> getParents(const std::string& commitHash) {
        std::vector<std::string> parents;
        try {
            auto obj = odb_.read(commitHash);
            if (obj.type != ObjectType::Commit) return parents;
            std::string data(obj.content.begin(), obj.content.end());
            std::istringstream iss(data);
            std::string line;
            while (std::getline(iss, line))
                if (line.substr(0, 7) == "parent ") parents.push_back(line.substr(7));
        } catch (...) {}
        return parents;
    }

    std::map<std::string, std::string> getTreeFiles(const std::string& treeHash) {
        std::map<std::string, std::string> files;
        try {
            auto entries = odb_.parseTree(treeHash);
            for (auto& e : entries) files[e.name] = e.hash;
        } catch (...) {}
        return files;
    }

    std::string buildTree(const std::map<std::string, std::string>& files) {
        std::vector<TreeEntry> entries;
        for (auto& [name, hash] : files) {
            TreeEntry e;
            e.mode = "100644";
            e.name = name;
            e.hash = hash;
            entries.push_back(e);
        }
        return odb_.storeTree(entries);
    }

    void resolveConflict(const std::string& file, const std::string& curHash,
                         const std::string& othHash, MergeResult& result) {
        auto curObj = odb_.read(curHash);
        auto othObj = odb_.read(othHash);
        auto curContent = std::string(curObj.content.begin(), curObj.content.end());
        auto othContent = std::string(othObj.content.begin(), othObj.content.end());

        auto lines = DiffEngine::splitLines(curContent);
        auto othLines = DiffEngine::splitLines(othContent);
        auto diffs = DiffEngine::diffLines(curContent, othContent);

        auto fullPath = workDir_ + "/" + file;
        std::ofstream f(fullPath);
        if (f) {
            f << "<<<<<<< HEAD (current)\n";
            f << curContent;
            if (!curContent.empty() && curContent.back() != '\n') f << '\n';
            f << "=======\n";
            f << othContent;
            if (!othContent.empty() && othContent.back() != '\n') f << '\n';
            f << ">>>>>>> merging branch\n";
        }
        result.success = false;
        std::cerr << "\033[31mConflict: " << file << "\033[0m\n";
    }
};

} // namespace vcs
