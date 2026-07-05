#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>

namespace vcs {

int cmd_cherry_pick(const std::map<std::string, std::string>& options,
                    const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    if (args.empty()) { out.error("Usage: vcs cherry-pick <commit>"); return 1; }

    Repository repo;
    auto commitHash = repo.refs().resolveRef(args[0]);
    if (commitHash.empty()) commitHash = repo.objects().resolveShortHash(args[0]);
    if (commitHash.empty()) { out.error("Unknown commit: " + args[0]); return 1; }

    if (!repo.cherryPick(commitHash)) {
        out.error("Cherry-pick failed");
        return 1;
    }

    out.success("Cherry-picked " + Reference::hashToShort(commitHash));
    return 0;
}

} // namespace vcs
