#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>

namespace vcs {

int cmd_clean(const std::map<std::string, std::string>& options,
              const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }

    Repository repo;
    bool dryRun = options.find("n") != options.end() || options.find("dry-run") != options.end();
    bool force = options.find("f") != options.end() || options.find("force") != options.end();
    bool removeDirs = options.find("d") != options.end();

    auto untracked = repo.getUntrackedFiles();
    if (untracked.empty()) { out.info("Nothing to clean"); return 0; }

    if (!force && !dryRun) {
        out.warn("Use -f to force clean");
        for (auto& f : untracked) out.println("  " + f, Output::DIM);
        return 1;
    }

    for (auto& f : untracked) {
        auto fullPath = repo.workDir() + "/" + f;
        if (FileSystem::isDirectory(fullPath) && !removeDirs) continue;
        if (dryRun) {
            out.println("Would remove " + f, Output::YELLOW);
        } else {
            if (FileSystem::isDirectory(fullPath)) FileSystem::removeAll(fullPath);
            else FileSystem::remove(fullPath);
            out.println("Removed " + f, Output::GREEN);
        }
    }
    return 0;
}

} // namespace vcs
