#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>

namespace vcs {

int cmd_rm(const std::map<std::string, std::string>& options,
           const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    if (args.empty()) { out.error("Usage: vcs rm [--cached] <file>..."); return 1; }

    Repository repo;
    repo.index().load();
    bool cached = options.find("cached") != options.end();
    int removed = 0;

    for (auto& arg : args) {
        if (!repo.index().hasFile(arg)) {
            out.warn(arg + " is not tracked");
            continue;
        }
        repo.index().unstage(arg);
        if (!cached) {
            auto fullPath = repo.workDir() + "/" + arg;
            if (FileSystem::exists(fullPath)) FileSystem::remove(fullPath);
        }
        out.println("Removed " + arg, Output::GREEN);
        ++removed;
    }

    repo.index().save();
    if (removed > 0) out.success("Removed " + std::to_string(removed) + " file(s)");
    return 0;
}

} // namespace vcs
