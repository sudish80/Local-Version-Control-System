#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>

namespace vcs {

int cmd_reset(const std::map<std::string, std::string>& options,
              const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    if (args.empty()) { out.error("Usage: vcs reset [--soft|--mixed|--hard] <commit>"); return 1; }

    std::string mode = "mixed";
    auto optIt = options.find("soft");
    if (optIt != options.end()) mode = "soft";
    optIt = options.find("mixed");
    if (optIt != options.end()) mode = "mixed";
    optIt = options.find("hard");
    if (optIt != options.end()) mode = "hard";

    Repository repo;
    auto headBefore = repo.refs().resolveHead();

    auto targetHash = repo.refs().resolveRef(args[0]);
    if (targetHash.empty()) targetHash = repo.objects().resolveShortHash(args[0]);
    if (targetHash.empty()) { out.error("Unknown target: " + args[0]); return 1; }

    if (mode == "hard") {
        repo.index().load();
        for (auto& e : repo.index().getAllFiles()) {
            auto fullPath = repo.workDir() + "/" + e.path;
            if (FileSystem::exists(fullPath)) FileSystem::remove(fullPath);
        }
    }

    if (!repo.reset(targetHash, mode)) {
        out.error("Reset failed");
        return 1;
    }

    out.success("Reset (" + mode + ") to " + Reference::hashToShort(targetHash));
    return 0;
}

} // namespace vcs
