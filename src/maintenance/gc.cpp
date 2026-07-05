#include "vcs/repository.hpp"
#include "vcs/output.hpp"

namespace vcs {

int cmd_gc(const std::map<std::string, std::string>& options,
           const std::vector<std::string>& args) {
    Output out;
    if (!Repository::isRepository()) { out.error("Not a VCS repository"); return 1; }
    Repository repo;
    auto removed = repo.gc();
    if (removed > 0) {
        out.success("Garbage collected " + std::to_string(removed) + " unreachable object(s)");
    } else {
        out.info("No unreachable objects found");
    }
    return 0;
}

} // namespace vcs
