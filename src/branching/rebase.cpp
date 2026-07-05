#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>

namespace vcs {

int cmd_rebase(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    if (args.empty()) { out.error("Usage: vcs rebase <branch>"); return 1; }

    std::string branch = args[0];
    Repository repo;

    auto branches = repo.refs().listBranches();
    bool found = false;
    for (auto& b : branches) {
        if (b == branch) { found = true; break; }
    }
    if (!found) { out.error("Branch not found: " + branch); return 1; }

    if (!repo.rebase(branch)) {
        out.error("Rebase failed");
        return 1;
    }

    out.success("Rebased onto " + branch);
    return 0;
}

} // namespace vcs
