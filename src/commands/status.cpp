#include "vcs/repository.hpp"
#include "vcs/output.hpp"

namespace vcs {

int cmd_status(const std::map<std::string, std::string>& options,
               const std::vector<std::string>& args) {
    if (!Repository::isRepository()) {
        Output out; out.error("Not a VCS repository"); return 1;
    }
    Repository repo;
    repo.printStatus();
    return 0;
}

} // namespace vcs
