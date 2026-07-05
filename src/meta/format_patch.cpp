#include "vcs/repository.hpp"
#include "vcs/output.hpp"

namespace vcs {

int cmd_format_patch(const std::map<std::string, std::string>& options,
                     const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    if (args.empty()) { out.error("Usage: vcs format-patch <commit>..."); return 1; }

    Repository repo;

    for (auto& arg : args) {
        auto hash = repo.refs().resolveRef(arg);
        if (hash.empty()) hash = repo.objects().resolveShortHash(arg);
        if (hash.empty()) { out.warn("Unknown commit: " + arg + ", skipping"); continue; }

        auto patch = repo.formatPatch(hash);
        if (patch.empty()) { out.warn("Could not format patch for " + arg); continue; }

        auto filename = Reference::hashToShort(hash) + ".patch";
        if (FileSystem::writeTextFile(filename, patch))
            out.println("Created: " + filename, Output::GREEN);
        else
            out.error("Failed to write " + filename);
    }

    return 0;
}

} // namespace vcs
