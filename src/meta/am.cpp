#include "vcs/repository.hpp"
#include "vcs/output.hpp"

namespace vcs {

int cmd_am(const std::map<std::string, std::string>& options,
           const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    if (args.empty()) { out.error("Usage: vcs am <patch-file>"); return 1; }

    Repository repo;
    std::string author;
    auto it = options.find("author");
    if (it != options.end()) author = it->second;

    if (!repo.am(args[0], author)) {
        out.error("Failed to apply patch: " + args[0]);
        return 1;
    }

    out.success("Applied " + args[0]);
    return 0;
}

} // namespace vcs
