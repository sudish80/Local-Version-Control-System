#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>
#include <sstream>
#include <map>
#include <set>

namespace vcs {

int cmd_revert(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args) {
    Output out;

    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    if (args.empty()) { out.error("Usage: vcs revert <commit>"); return 1; }

    Repository repo;

    std::string targetHash = repo.refs().resolveRef(args[0]);
    if (targetHash.empty()) targetHash = repo.objects().resolveShortHash(args[0]);
    if (targetHash.empty()) targetHash = args[0];
    if (targetHash.size() < 7) { out.error("Invalid commit reference"); return 1; }

    try {
        auto commitObj = repo.objects().read(targetHash);
        if (commitObj.type != ObjectType::Commit) { out.error("Not a commit: " + args[0]); return 1; }

        std::string data(commitObj.content.begin(), commitObj.content.end());
        std::istringstream iss(data);
        std::string line, treeHash, parentHash;
        while (std::getline(iss, line)) {
            if (line.substr(0, 5) == "tree ") treeHash = line.substr(5);
            else if (line.substr(0, 7) == "parent ") parentHash = line.substr(7);
        }

        if (treeHash.empty()) { out.error("Cannot revert: no tree"); return 1; }

        std::map<std::string, std::string> commitFiles;
        for (auto& e : repo.objects().parseTree(treeHash))
            commitFiles[e.name] = e.hash;

        std::map<std::string, std::string> parentFiles;
        if (!parentHash.empty()) {
            auto parentObj = repo.objects().read(parentHash);
            std::string pd(parentObj.content.begin(), parentObj.content.end());
            std::istringstream iss2(pd);
            std::string parentTree;
            while (std::getline(iss2, line))
                if (line.substr(0, 5) == "tree ") { parentTree = line.substr(5); break; }
            if (!parentTree.empty())
                for (auto& e : repo.objects().parseTree(parentTree))
                    parentFiles[e.name] = e.hash;
        }

        repo.index().load();
        bool anyChange = false;

        std::set<std::string> allFiles;
        for (auto& [k, _] : commitFiles) allFiles.insert(k);
        for (auto& [k, _] : parentFiles) allFiles.insert(k);

        for (auto& name : allFiles) {
            auto itC = commitFiles.find(name);
            auto itP = parentFiles.find(name);
            bool same = (itC != commitFiles.end() && itP != parentFiles.end() && itC->second == itP->second);
            if (same) continue;

            auto fullPath = repo.workDir() + "/" + name;

            if (itC != commitFiles.end() && itP == parentFiles.end()) {
                if (FileSystem::exists(fullPath)) {
                    FileSystem::remove(fullPath);
                    repo.index().unstage(name);
                    out.println("Removed " + name + " (added by commit, now removed)", Output::GREEN);
                    anyChange = true;
                }
            } else {
                auto sourceHash = (itP != parentFiles.end()) ? itP->second : itC->second;
                auto sourceObj = repo.objects().read(sourceHash);
                FileSystem::writeFile(fullPath, sourceObj.content);
                auto mtime = FileSystem::getLastModifiedTime(fullPath);
                auto size = FileSystem::getFileSize(fullPath);
                repo.index().stage(name, sourceHash, mtime, size);
                out.println("Restored " + name + " to pre-commit state", Output::GREEN);
                anyChange = true;
            }
        }

        repo.index().save();

        if (anyChange) {
            out.success("Working tree updated to revert " + Reference::hashToShort(targetHash));
        } else {
            out.info("No changes to revert");
        }
        return 0;
    } catch (std::exception& e) {
        out.error(std::string("Revert failed: ") + e.what());
        return 1;
    }
}

} // namespace vcs
