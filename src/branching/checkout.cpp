#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>

namespace vcs {

int cmd_checkout(const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    if (args.empty()) { out.error("Usage: vcs checkout <branch>"); return 1; }

    Repository repo;

    auto target = args[0];
    auto branches = repo.refs().listBranches();
    bool isBranch = false;
    std::string targetHash;

    for (auto& b : branches) {
        if (b == target) { isBranch = true; break; }
    }

    if (isBranch) {
        targetHash = repo.refs().getBranchRef(target);
        if (targetHash.empty()) { out.error("Branch has no commits"); return 1; }
    } else {
        targetHash = repo.refs().resolveRef(target);
        if (targetHash.empty()) targetHash = repo.objects().resolveShortHash(target);
        if (targetHash.empty()) { out.error("Not a valid reference: " + target); return 1; }
    }

    try {
        auto commitObj = repo.objects().read(targetHash);
        if (commitObj.type != ObjectType::Commit) { out.error("Not a commit"); return 1; }

        std::string data(commitObj.content.begin(), commitObj.content.end());
        std::istringstream iss(data);
        std::string line, treeHash;
        while (std::getline(iss, line))
            if (line.substr(0, 5) == "tree ") { treeHash = line.substr(5); break; }

        if (treeHash.empty()) { out.error("Commit has no tree"); return 1; }

        repo.index().load();
        auto treeEntries = repo.objects().parseTree(treeHash);

        repo.index().clear();
        for (auto& e : treeEntries) {
            auto fullPath = repo.workDir() + "/" + e.name;
            try {
                auto obj = repo.objects().read(e.hash);
                FileSystem::writeFile(fullPath, obj.content);
            } catch (...) {}
            auto mtime = FileSystem::getLastModifiedTime(fullPath);
            auto size = FileSystem::getFileSize(fullPath);
            repo.index().stage(e.name, e.hash, mtime, size);
        }
        repo.index().save();

        if (isBranch) {
            repo.refs().setCurrentBranch(target);
            out.success("Switched to branch " + target);
        } else {
            repo.refs().setHead(targetHash);
            out.success("HEAD is now at " + Reference::hashToShort(targetHash));
        }
    } catch (std::exception& e) {
        out.error(std::string("Checkout failed: ") + e.what());
        return 1;
    }
    return 0;
}

} // namespace vcs
