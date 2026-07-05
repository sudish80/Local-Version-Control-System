#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>

namespace vcs {

int cmd_tag(const std::map<std::string, std::string>& options,
            const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }

    Repository repo;

    if (options.find("d") != options.end() || options.find("delete") != options.end()) {
        if (args.empty()) { out.error("Usage: vcs tag -d <name>"); return 1; }
        if (repo.refs().deleteTag(args[0])) out.success("Deleted tag " + args[0]);
        else out.error("Tag not found: " + args[0]);
        return 0;
    }

    if (args.empty()) {
        auto tags = repo.refs().listTags();
        if (tags.empty()) out.info("No tags");
        else for (auto& t : tags) out.println(t, Output::CYAN);
        return 0;
    }

    auto name = args[0];
    std::string hash;
    if (args.size() >= 2) hash = args[1];
    if (hash.empty()) hash = repo.refs().resolveHead();
    if (hash.empty()) { out.error("No commits to tag"); return 1; }

    if (repo.refs().setTagRef(name, hash)) out.success("Created tag " + name);
    else out.error("Failed to create tag");
    return 0;
}

} // namespace vcs
