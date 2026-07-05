#include "vcs/repository.hpp"
#include "vcs/merge.hpp"
#include "vcs/output.hpp"
#include <iostream>

namespace vcs {

int cmd_merge(const std::map<std::string, std::string>& options,
              const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    if (args.empty()) { out.error("Usage: vcs merge <branch>"); return 1; }

    Repository repo;

    auto branchName = args[0];
    auto currentHash = repo.refs().resolveHead();
    if (currentHash.empty()) { out.error("Nothing to merge onto"); return 1; }

    auto otherHash = repo.refs().resolveRef(branchName);
    if (otherHash.empty()) { out.error("Branch not found: " + branchName); return 1; }

    auto currentBranch = repo.refs().getCurrentBranch();
    if (currentBranch.empty()) currentBranch = "HEAD";

    MergeEngine engine(repo.objects(), repo.index(), repo.workDir());
    auto result = engine.merge(currentHash, otherHash, currentBranch, branchName);

    if (!result.success) {
        out.error("Merge failed: " + result.message);
        return 1;
    }

    if (result.fastForward) {
        auto branch = repo.refs().getCurrentBranch();
        if (!branch.empty()) {
            repo.refs().setBranchRef(branch, otherHash);
            repo.refs().setCurrentBranch(branch);
        } else {
            repo.refs().setHead(otherHash);
        }

        try {
            auto commitObj = repo.objects().read(otherHash);
            std::string data(commitObj.content.begin(), commitObj.content.end());
            std::istringstream iss(data);
            std::string line, treeHash;
            while (std::getline(iss, line))
                if (line.substr(0, 5) == "tree ") { treeHash = line.substr(5); break; }

            if (!treeHash.empty()) {
                auto treeEntries = repo.objects().parseTree(treeHash);
                repo.index().load();
                repo.index().clear();
                for (auto& e : treeEntries) {
                    auto fullPath = repo.workDir() + "/" + e.name;
                    repo.objects().read(e.hash);
                    auto mtime = FileSystem::getLastModifiedTime(fullPath);
                    auto size = FileSystem::getFileSize(fullPath);
                    repo.index().stage(e.name, e.hash, mtime, size);
                }
                repo.index().save();
            }
        } catch (...) {}

        out.success("Fast-forward merged " + branchName + " into " + currentBranch);
    } else {
        auto mergeHash = repo.objects().storeCommit(result.mergeHash, currentHash,
            "Merge <merge@vcs.local>", result.message);

        auto branch = repo.refs().getCurrentBranch();
        if (!branch.empty()) {
            repo.refs().setBranchRef(branch, mergeHash);
        } else {
            repo.refs().setHead(mergeHash);
        }
        out.success("Merged " + branchName + " into " + currentBranch +
                    " (" + Reference::hashToShort(mergeHash) + ")");
    }
    return 0;
}

} // namespace vcs
