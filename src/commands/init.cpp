#include "vcs/repository.hpp"
#include "vcs/output.hpp"
#include <iostream>

namespace vcs {

int cmd_init(const std::vector<std::string>& args) {
    Output out;
    std::string path;
    if (!args.empty()) path = args[0];

    Repository repo(path);
    if (repo.init()) {
        out.success("Initialized empty VCS repository in " + repo.vcsDir());
        return 0;
    } else {
        out.error("Repository already exists in " + repo.vcsDir());
        return 1;
    }
}

} // namespace vcs
