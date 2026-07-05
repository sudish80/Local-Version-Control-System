#include "vcs/repository.hpp"
#include "vcs/output.hpp"

namespace vcs {

int cmd_fsck(const std::map<std::string, std::string>& options,
             const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    Repository repo;
    return repo.fsck();
}

} // namespace vcs
