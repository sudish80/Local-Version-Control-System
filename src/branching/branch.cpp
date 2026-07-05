#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>

namespace vcs {

int cmd_branch(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }

    Repository repo;

    if (options.find("d") != options.end() || options.find("delete") != options.end()) {
        if (args.empty()) { out.error("Usage: vcs branch -d <name>"); return 1; }
        auto name = args[0];
        auto current = repo.refs().getCurrentBranch();
        if (name == current) { out.error("Cannot delete current branch"); return 1; }
        if (repo.refs().deleteBranch(name)) {
            out.success("Deleted branch " + name);
        } else {
            out.error("Branch not found: " + name);
        }
        return 0;
    }

    if (args.empty()) {
        auto current = repo.refs().getCurrentBranch();
        auto branches = repo.refs().listBranches();
        if (branches.empty()) { out.info("No branches"); return 0; }
        for (auto& b : branches) {
            if (b == current) out.println("* " + b, Output::GREEN);
            else out.println("  " + b);
        }
        return 0;
    }

    auto name = args[0];
    auto headHash = repo.refs().resolveHead();
    if (headHash.empty()) { out.error("No commits to branch from"); return 1; }

    if (repo.refs().setBranchRef(name, headHash)) {
        out.success("Created branch " + name);
    } else {
        out.error("Failed to create branch");
    }
    return 0;
}

} // namespace vcs
