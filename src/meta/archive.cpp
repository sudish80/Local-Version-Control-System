#include "vcs/repository.hpp"
#include "vcs/output.hpp"

namespace vcs {

int cmd_archive(const std::map<std::string, std::string>& options,
                const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    if (args.empty()) { out.error("Usage: vcs archive <commit> [-o <output.zip>]"); return 1; }

    Repository repo;
    auto hash = repo.refs().resolveRef(args[0]);
    if (hash.empty()) hash = repo.objects().resolveShortHash(args[0]);
    if (hash.empty()) { out.error("Unknown commit: " + args[0]); return 1; }

    std::string outputFile = "archive.zip";
    auto it = options.find("o");
    if (it == options.end()) it = options.find("output");
    if (it != options.end()) outputFile = it->second;

    if (!repo.archive(hash, outputFile)) {
        out.error("Archive failed");
        return 1;
    }

    out.success("Created " + std::string(outputFile));
    return 0;
}

} // namespace vcs
